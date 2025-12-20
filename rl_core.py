import socket
import os
import struct
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import logging
import torch.optim as optim
from collections import deque

# ================= CONFIG =================

SOCKET_PATH = os.path.expanduser("~/testing.sock")

STATE_DIM = 99
NUM_MUTATIONS = 3
TIER_DIM = 3
MAX_MUTATIONS = 15

HIDDEN_SIZE = 256

UPDATE_INTERVAL = 128
BATCH_SIZE = 64

GAMMA = 0.99
LAMBDA = 0.95
CLIP_EPS = 0.2
ENTROPY_COEF = 0.03
VALUE_LOSS_COEF = 0.3
MAX_GRAD_NORM = 0.5
LR_ACTOR = 3e-4
LR_CRITIC = 1e-3
EPOCHS = 3

REWARD_CLIP = 50.0
LOG_INTERVAL = 100
SAVE_INTERVAL = 1000
MODEL_PATH = "ppo_model.pt"
BEST_MODEL_PATH = "ppo_model_best.pt"

MUTATION_COUNTS = [10, 5, 15]

# ================= LOGGING =================
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('rl_agent.log')
    ]
)
logger = logging.getLogger(__name__)

# ================= NETWORKS =================

def masked_logits(logits: torch.Tensor, valid: int) -> torch.Tensor:
    """Mask invalid actions with large negative values"""
    valid = min(valid, logits.shape[-1])
    valid = max(valid, 1)

    mask = torch.full_like(logits, -1e9)
    mask[..., :valid] = 0
    return logits + mask


class ActorNetwork(nn.Module):
    def __init__(self, state_dim, hidden=HIDDEN_SIZE):
        super().__init__()

        self.encoder = nn.Sequential(
            nn.Linear(state_dim, hidden),
            nn.LayerNorm(hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.LayerNorm(hidden),
            nn.ReLU()
        )

        self.tier_heads = nn.ModuleList(
            [nn.Linear(hidden, TIER_DIM) for _ in range(NUM_MUTATIONS)]
        )
        
        self.mut_heads = nn.ModuleList(
            [nn.Linear(hidden, MAX_MUTATIONS) for _ in range(NUM_MUTATIONS)]
        )

    def forward(self, state):
        features = self.encoder(state)

        tiers = [h(features) for h in self.tier_heads]
        muts = [h(features) for h in self.mut_heads]

        return tiers, muts


class CriticNetwork(nn.Module):
    def __init__(self, state_dim, hidden=HIDDEN_SIZE):
        super().__init__()

        self.net = nn.Sequential(
            nn.Linear(state_dim, hidden),
            nn.LayerNorm(hidden),
            nn.ReLU(),
            nn.Linear(hidden, hidden),
            nn.LayerNorm(hidden),
            nn.ReLU(),
            nn.Linear(hidden, 1)
        )

    def forward(self, state):
        return self.net(state).squeeze(-1)


# ================= PPO AGENT =================

class PPOAgent:
    def __init__(self, state_dim):
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        logger.info(f"Using device: {self.device}")

        self.actor = ActorNetwork(state_dim).to(self.device)
        self.critic = CriticNetwork(state_dim).to(self.device)

        self.actor_opt = optim.Adam(self.actor.parameters(), lr=LR_ACTOR)
        self.critic_opt = optim.Adam(self.critic.parameters(), lr=LR_CRITIC)

        self.reset_buffer()

    def reset_buffer(self):
        self.states = []
        self.actions = []
        self.rewards = []
        self.log_probs = []
        self.values = []
        self.dones = []

    @torch.no_grad()
    def select_actions(self, state):
        """
        Select actions for NUM_MUTATIONS mutation steps.
        Returns: (actions, log_prob, value, entropy)
        """
        state = torch.FloatTensor(state).unsqueeze(0).to(self.device)
        tiers, muts = self.actor(state)
        value = self.critic(state)

        actions = []
        log_probs_list = []
        entropies_list = []

        for i in range(NUM_MUTATIONS):
            tier_dist = torch.distributions.Categorical(logits=tiers[i])
            tier = tier_dist.sample()
            tier_item = tier.item()

            mut_logits_masked = masked_logits(muts[i], MUTATION_COUNTS[tier_item])
            mut_dist = torch.distributions.Categorical(logits=mut_logits_masked)
            mut = mut_dist.sample()

            actions.extend([tier_item, mut.item()])
            log_probs_list.append(tier_dist.log_prob(tier) + mut_dist.log_prob(mut))
            entropies_list.append(tier_dist.entropy() + mut_dist.entropy())

        log_prob_total = torch.stack(log_probs_list).sum().item()
        entropy_total = torch.stack(entropies_list).mean().item()
        
        return actions, log_prob_total, value.item(), entropy_total

    def store(self, state, actions, reward, log_probs, value, done):
        self.states.append(state)
        self.actions.append(actions)
        self.rewards.append(reward)
        self.log_probs.append(log_probs)
        self.values.append(value)
        self.dones.append(done)

    def compute_gae(self):
        """Compute returns and advantages using GAE"""
        returns = []
        advantages = []
        gae = 0.0
        next_value = 0.0

        for i in reversed(range(len(self.rewards))):
            if self.dones[i]:
                gae = 0.0
                next_value = 0.0

            delta = self.rewards[i] + GAMMA * next_value - self.values[i]
            gae = delta + GAMMA * LAMBDA * gae

            advantages.append(gae)
            returns.append(gae + self.values[i])
            next_value = self.values[i]

        advantages = advantages[::-1]
        returns = returns[::-1]

        advantages = torch.tensor(advantages, dtype=torch.float32, device=self.device)
        returns = torch.tensor(returns, dtype=torch.float32, device=self.device)

        advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)
        advantages = torch.clamp(advantages, -5.0, 5.0)
        
        return returns, advantages

    def update(self):
        """PPO update with proper tensor handling"""
        if len(self.states) < BATCH_SIZE:
            return None, None

        states = torch.FloatTensor(np.array(self.states, dtype=np.float32)).to(self.device)
        actions = torch.LongTensor(np.array(self.actions, dtype=np.int64)).to(self.device)
        old_log_probs = torch.FloatTensor(np.array(self.log_probs, dtype=np.float32)).to(self.device)

        returns, advantages = self.compute_gae()
        
        total_actor_loss = 0.0
        total_critic_loss = 0.0
        num_updates = 0

        indices = np.arange(len(self.states))

        for epoch in range(EPOCHS):
            np.random.shuffle(indices)

            for start in range(0, len(indices), BATCH_SIZE):
                end = min(start + BATCH_SIZE, len(indices))
                batch_indices = indices[start:end]
                batch_size = len(batch_indices)

                tiers, muts = self.actor(states[batch_indices])
                values = self.critic(states[batch_indices])

                batch_log_probs = torch.zeros(batch_size, device=self.device, dtype=torch.float32)
                batch_entropies = torch.zeros(batch_size, device=self.device, dtype=torch.float32)

                for mutation_idx in range(NUM_MUTATIONS):
                    tier_logits = tiers[mutation_idx]
                    tier_dist = torch.distributions.Categorical(logits=tier_logits)
                    tier_actions = actions[batch_indices, 2 * mutation_idx]
                    
                    batch_log_probs += tier_dist.log_prob(tier_actions)
                    batch_entropies += tier_dist.entropy()

                    mut_logits = muts[mutation_idx]
                    mut_actions = actions[batch_indices, 2 * mutation_idx + 1]
                    
                    masked_mut_logits = torch.zeros_like(mut_logits)
                    for j in range(batch_size):
                        tier_val = tier_actions[j].item()
                        masked_mut_logits[j] = masked_logits(
                            mut_logits[j:j+1], 
                            MUTATION_COUNTS[tier_val]
                        ).squeeze(0)
                    
                    mut_dist = torch.distributions.Categorical(logits=masked_mut_logits)
                    batch_log_probs += mut_dist.log_prob(mut_actions)
                    batch_entropies += mut_dist.entropy()

                ratio = torch.exp(batch_log_probs - old_log_probs[batch_indices])
                surr1 = ratio * advantages[batch_indices]
                surr2 = torch.clamp(
                    ratio,
                    1.0 - CLIP_EPS,
                    1.0 + CLIP_EPS
                ) * advantages[batch_indices]

                actor_loss = -torch.min(surr1, surr2).mean()
                actor_loss = actor_loss - ENTROPY_COEF * batch_entropies.mean()

                critic_loss = F.mse_loss(values, returns[batch_indices])

                self.actor_opt.zero_grad()
                actor_loss.backward()
                nn.utils.clip_grad_norm_(self.actor.parameters(), MAX_GRAD_NORM)
                self.actor_opt.step()

                self.critic_opt.zero_grad()
                critic_loss.backward()
                nn.utils.clip_grad_norm_(self.critic.parameters(), MAX_GRAD_NORM)
                self.critic_opt.step()

                total_actor_loss += actor_loss.item()
                total_critic_loss += critic_loss.item()
                num_updates += 1

        self.reset_buffer()
        
        if num_updates > 0:
            return total_actor_loss / num_updates, total_critic_loss / num_updates
        return None, None

    def save(self, path=MODEL_PATH):
        torch.save(
            {
                "actor": self.actor.state_dict(),
                "critic": self.critic.state_dict(),
                "actor_opt": self.actor_opt.state_dict(),
                "critic_opt": self.critic_opt.state_dict(),
            }, path)
        logger.info(f"✅ Saved model to {path}")

    def load(self, path=MODEL_PATH):
        if os.path.exists(path):
            ckpt = torch.load(path, map_location=self.device)
            self.actor.load_state_dict(ckpt["actor"])
            self.critic.load_state_dict(ckpt["critic"])
            self.actor_opt.load_state_dict(ckpt["actor_opt"])
            self.critic_opt.load_state_dict(ckpt["critic_opt"])
            logger.info(f"✅ Loaded PPO model from {path}")
            return True
        return False


# ================= SOCKET =================

def normalize_state(state):
    """Normalize state to prevent NaN/Inf issues"""
    state = np.nan_to_num(
        state,
        nan=0.0,
        posinf=1e3,
        neginf=-1e3,
    )
    state = np.clip(state, -10.0, 10.0)
    return state


if os.path.exists(SOCKET_PATH):
    os.remove(SOCKET_PATH)

server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(SOCKET_PATH)
server.listen(1)
logger.info("🐍 PPO server waiting for fuzzer...")

conn, _ = server.accept()
logger.info("✅ Fuzzer connected")


def recv_msg():
    """Receive message from fuzzer"""
    header = conn.recv(1)
    if not header:
        return None, None

    msg_type = header[0]

    if msg_type == 0:
        size = struct.unpack("I", conn.recv(4))[0]
        data = struct.unpack(
            "f" * size,
            conn.recv(4 * size),
        )
        return "state", np.array(data, dtype=np.float32)

    if msg_type == 1:
        reward = struct.unpack("f", conn.recv(4))[0]
        return "reward", reward

    return None, None


def send_actions(actions):
    """Send actions to fuzzer"""
    conn.sendall(struct.pack("i" * len(actions), *actions))


# ================= MAIN LOOP =================

agent = PPOAgent(STATE_DIM)
agent.load()

step = 0
episode_reward = 0.0
reward_history = deque(maxlen=100)
last = {}

try:
    while True:
        msg_type, data = recv_msg()
        if msg_type is None:
            break

        if msg_type == "state":
            state = normalize_state(data)
            actions, log_probs, value, entropy = agent.select_actions(state)
            send_actions(actions)

            last = {
                "state": state,
                "actions": actions,
                "log_probs": log_probs,
                "value": value,
            }

        elif msg_type == "reward":
            reward = data
            episode_reward += reward

            done = ((step + 1) % UPDATE_INTERVAL) == 0

            agent.store(
                last["state"],
                last["actions"],
                reward,
                last["log_probs"],
                last["value"],
                done,
            )

            step += 1

            if done:
                actor_loss, critic_loss = agent.update()
                reward_history.append(episode_reward)
                avg_reward = sum(reward_history) / len(reward_history)

                if actor_loss is not None:
                    logger.info(
                        f"[{step:6d}] "
                        f"Reward={episode_reward:8.2f} "
                        f"Avg={avg_reward:7.2f} "
                        f"A_Loss={actor_loss:7.4f} "
                        f"C_Loss={critic_loss:7.4f}"
                    )

                episode_reward = 0.0

except KeyboardInterrupt:
    logger.info("\n⛔ Interrupted")

finally:
    agent.save()
    conn.close()
    server.close()
    if os.path.exists(SOCKET_PATH):
        os.remove(SOCKET_PATH)
    logger.info("✅ Shutdown complete")
