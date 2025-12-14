import socket
import os
import struct
import numpy as np
import random
import torch
import torch.nn as nn
import torch.optim as optim
import csv

LOG_FILE = "ppo_training_log.csv"

# == socket setup ==
SOCKET_PATH = os.path.expanduser("~/testing.sock")
ACTION_DIM = 30
STATE_DIM = 95
NUM_ACTIONS = 3
UPDATE_INTERVAL = 128
HIDDEN_SIZE = 128

class ActorNetwork(nn.Module):
    def __init__(self, state_dim, action_dim, hidden = HIDDEN_SIZE):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, action_dim),
            nn.Softmax(dim=-1)
        )
    
    def forward(self, state):
        return self.net(state)

class CriticNetwork(nn.Module):
    def __init__(self, state_dim, hidden=HIDDEN_SIZE):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.ReLU(),
            nn.Linear(hidden, 1)
        )

    def forward(self, state):
        return self.net(state)

#== ppo guy ==

class PPOAgent:
    def __init__(self, state_dim, action_dim, num_actions = NUM_ACTIONS):
        self.state_dim = state_dim
        self.action_dim = action_dim
        self.num_actions = num_actions

        self.actor = ActorNetwork(state_dim, action_dim)
        self.critic = CriticNetwork(state_dim)
        
        self.actor_optimizer = optim.Adam(self.actor.parameters(), lr=3e-4)
        self.critic_optimizer = optim.Adam(self.critic.parameters(), lr=1e-3)

        self.gamma = 0.99
        self.epsilon = 0.2
        self.epochs = 10
        self.batch_size = 64

        self.states = []
        self.actions = []
        self.rewards = []
        self.log_probs = []
        self.values = []
        self.dones = []

    def select_actions(self, state):
        state_tensor = torch.FloatTensor(state).unsqueeze(0)

        with torch.no_grad(): #no need to turnt s into grad, not training data
            probs = self.actor(state_tensor)
            value = self.critic(state_tensor)

        if np.random.rand() < 0.2:
            actions = [np.random.randint(0, self.action_dim) for _ in range(self.num_actions)]
            dist = torch.distributions.Categorical(probs)
            log_probs = [dist.log_prob(torch.tensor(a)).item() for a in actions]
            return actions, log_probs, value.item()

        dist = torch.distributions.Categorical(probs) #this turns probs into "dice" based on vals, 
        
        actions = []
        log_probs = []

        for _ in range(self.num_actions):
            action = dist.sample()
            actions.append(action.item())
            log_probs.append(dist.log_prob(action).item())

        return actions, log_probs, value.item() #converts into usable nums 


    def store_experience(self, state, actions, reward, log_probs, value, done):
        self.states.append(state)
        self.actions.append(actions)
        self.rewards.append(reward)
        self.log_probs.append(log_probs)
        self.values.append(value)
        self.dones.append(done)

    def compute_returns_and_advantages(self):
        returns = []
        G = 0

        for i in reversed(range(len(self.rewards))):
            if self.dones[i]:
                G = 0
            G = self.rewards[i] + self.gamma * G
            returns.insert(0, G)

        returns = torch.FloatTensor(returns)
        values = torch.FloatTensor(self.values)
        advantages = returns - values
        advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

        return returns, advantages

    def update(self):
        if len(self.states) < self.batch_size:
            return None, None

        states = torch.FloatTensor(np.array(self.states))
        returns, advantages = self.compute_returns_and_advantages()

        all_actions = []
        all_old_log_probs = []
        all_advantages = []
        all_states= []

        for i in range(len(self.states)):
            for j in range(self.num_actions):
                all_states.append(self.states[i])
                all_actions.append(self.actions[i][j])
                all_old_log_probs.append(self.log_probs[i][j])
                all_advantages.append(advantages[i].item())

        all_states = torch.FloatTensor(self.states[i])
        all_actions = torch.LongTensor(all_actions)
        all_old_log_probs = torch.FloatTensor(all_old_log_probs)
        all_advantages = torch.FloatTensor(all_advantages)

        for _ in range(self.epochs):
            probs = self.actor(all_states)
            dist = torch.distributions.Categorical(probs)
            new_log_probs = dist.log_prob(all_actions)
            entropy = dist.entropy().mean()

            ratio = torch.exp(new_log_probs - all_old_log_probs)
            surr1 = ratio * all_advantages
            surr2 = torch.clamp(ratio, 1 - self.epsilon, 1+self.epsilon) * all_advantages

            actor_loss = -torch.min(surr1, surr2).mean() - 0.05 * entropy #turn program from maxxing F to negating -F, since optimizer step minimizes the loss and we want to max it

            
            self.actor_optimizer.zero_grad()
            actor_loss.backward()
            self.actor_optimizer.step()

        for _ in range(self.epochs):
            values = self.critic(states).squeeze()
            critic_loss = nn.MSELoss()(values, returns)

            self.critic_optimizer.zero_grad()
            critic_loss.backward()
            self.critic_optimizer.step()

        self.states.clear()
        self.actions.clear()
        self.rewards.clear()
        self.log_probs.clear()
        self.values.clear()
        self.dones.clear()

        return actor_loss.item(), critic_loss.item()

    def save(self, path="ppo_model"):
        torch.save({
            'actor':self.actor.state_dict(),
            'critic':self.critic.state_dict(),
        }, f"{path}.pt")
        print(f"Model saved to {path}.pt")

    def load(self, path="ppo_model"):
        if os.path.exists(f"{path}.pt"):
            checkpoint = torch.load(f"{path}.pt")
            self.actor.load_state_dict(checkpoint['actor'])
            self.critic.load_state_dict(checkpoint['critic'])
            print(f"Model loaded from {path}.pt")
            return True
        return False



# ==== Socket setup =====
#
server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(SOCKET_PATH)
server.listen(1)
print("Python server waiting for connection...")

conn, _ = server.accept()
print("Fuzzer connected.")


# ----------------- Message Handling -----------------
def receive_message(conn):
    header = conn.recv(1)
    if not header:
        return None, None
    msg_type = header[0]

    if msg_type == 0:
        size_raw = conn.recv(4)
        if len(size_raw) < 4:
            return None, None
        (num_floats,) = struct.unpack("I", size_raw)
        data = b''
        while len(data) < num_floats * 4:
            chunk = conn.recv(num_floats*4 - len(data))
            if not chunk:
                return None, None
            data += chunk
        state = np.array(struct.unpack("f"*num_floats, data))
        return "state", state

    elif msg_type == 1:
        data = conn.recv(4)
        if len(data) < 4:
            return None, None
        (reward,) = struct.unpack("f", data)
        return "reward", reward
    else:
        return None, None

def send_actions(conn, actions):
    # For now, send one int
    data = struct.pack("i" * len(actions), *actions)
    conn.sendall(data)



def normalize_state(state):
    state = np.clip(state, -1e6, 1e6)
    max_val = np.abs(state).max()
    if max_val > 1:
        state = state / max_val
    return state




agent = PPOAgent(STATE_DIM, ACTION_DIM, NUM_ACTIONS)
agent.load()

last_state = None
last_action = None
last_log_prob = None
last_value = None

step = 0
episode_reward = 0
total_rewards = []

def init_csv_log():
    with open(LOG_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['step', 'reward', 'actor_loss', 'critic_loss', 'avg_reward_10'])

def log_to_csv(step, reward, actor_loss, critic_loss, avg_reward):
    with open(LOG_FILE, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([step, f'{reward:.2f}', f'{actor_loss:.6f}', f'{critic_loss:.4f}', f'{avg_reward:.2f}'])


init_csv_log()

try:
    while True:
        msg_type, data = receive_message(conn)

        if msg_type is None:
            print("Client disconnected or invalid message")
            break
        if msg_type == "state":
            current_state = normalize_state(data)

            actions, log_probs, value = agent.select_actions(current_state)
            send_actions(conn, actions)

            last_state = current_state
            last_actions = actions
            last_log_probs = log_probs
            last_value = value

        elif msg_type == "reward":
            reward = data
            episode_reward += reward

            if last_state is not None and last_actions is not None:
                done = (step % UPDATE_INTERVAL == 0)
                agent.store_experience(
                    last_state, last_actions, reward,
                    last_log_probs, last_value, done
                )

                step += 1

                if step % UPDATE_INTERVAL == 0:
                    losses = agent.update()
                    if losses[0] is not None:
                        avg = np.mean(total_rewards[-10:]) if total_rewards else 0
                        log_to_csv(step, episode_reward, losses[0], losses[1], avg)
                        print(f"Step {step:6d} | "
                            f"Reward: {episode_reward:7.2f} | "
                            f"Actor Loss: {losses[0]:7.4f} | "
                            f"Critic Loss: {losses[1]:7.4f}")
                        
                    total_rewards.append(episode_reward)
                    episode_reward = 0
                    
                if step % 1000 == 0:
                    print(f"----- Step {step} | Avg reward (last 10): {avg:.2f} ----")

except KeyboardInterrupt:
    print("\n\nInterrupted by user")

finally:
    print(f"\nTotal steps: {step}")
    if total_rewards:
        print(f"Average reward {np.mean(total_rewards):.2f}")
    agent.save()
    conn.close()
    server.close()

    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)
