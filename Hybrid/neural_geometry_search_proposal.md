# Neural Geometry for Coverage-Guaranteed Search

## Research Directions with Technical Justification

---

# Part I: Mathematical Foundations

## 1.1 Search Theory Fundamentals

The mathematical foundation for search operations comes from WWII naval search theory, formalized by Koopman (1946) and refined by Washburn (2014). The key quantity is the **probability of detection (POD)**:

$$POD = 1 - \exp(-C)$$

where $C = \text{coverage} = Wvt/A$

Here $W$ is sweep width (effective sensor footprint), $v$ is vehicle speed, $t$ is search time, and $A$ is area. This exponential random search formula assumes targets are uniformly distributed—for non-uniform $P(x,y)$, the formula becomes:

$$POD = \iint P(x,y) \times (1 - \exp(-C(x,y))) \, dx \, dy$$

The optimization problem is: given limited time/fuel, choose which regions to search and in what order to maximize expected POD. This is fundamentally a resource allocation problem over continuous space.

## 1.2 Why This Is Hard

The desert search problem combines several NP-hard subproblems:

1. **Coverage path planning:** Given a region, find a path that covers it completely with minimum travel. Even for simple polygons, this is related to the lawn mowing problem (NP-hard).

2. **Orienteering problem:** Select which locations to visit to maximize collected reward under a budget constraint. NP-hard, generalizes knapsack.

3. **Vehicle routing:** Multiple vehicles, each with capacity/range constraints, must collectively cover demand. NP-hard even for simple variants.

4. **Continuous spatial decisions:** Unlike standard VRP where nodes are given, we must decide WHERE to search, not just in what order. This adds geometric complexity.

## 1.3 The Discretization Trade-off

To make the problem tractable, we discretize: divide the region into cells, treat cell centers as "nodes," solve a VRP-like problem. But discretization introduces error—coarse cells miss fine structure in $P(x,y)$, fine cells explode the problem size. A 100×100 km region at 1 km resolution gives 10,000 cells—far beyond what exact solvers can handle, but learnable structure remains.

---

# Part II: Why Neural Networks Should Help

## 2.1 The Fundamental Argument: Amortized Computation

The core insight behind neural combinatorial optimization is **amortized computation**. Traditional solvers compute solutions from scratch for each instance. A neural network, once trained on many instances, can leverage patterns learned across all of them.

**Analogy:** Consider how humans solve TSP instances. An expert doesn't run Concorde in their head—they recognize patterns: "convex hull first," "avoid crossing edges," "cluster nearby cities." These heuristics are learned from experience. A neural network learns similar (potentially better) heuristics from data.

**Formal statement:** Let $D$ be a distribution over problem instances. Training cost is $O(T \times |\text{training\_set}|)$. Inference cost is $O(\text{forward\_pass})$. If you solve $N$ instances from $D$, neural approach is worthwhile when:

$$N \times O(\text{forward\_pass}) + O(\text{training}) < N \times O(\text{traditional\_solver})$$

For large $N$ and fast forward pass, the neural approach wins even with expensive training. For desert SAR, if you're planning missions daily over similar terrain, $N$ is large.

## 2.2 What Structure Can the Network Learn?

The network doesn't just memorize solutions—it learns generalizable patterns. For probability-weighted search, learnable structure includes:

### Spatial Correlations in P(x,y)

Probability distributions aren't random—they have structure. A lost hiker's distribution clusters around last known position, trails, water sources. The network learns: "when probability peaks here, also check these correlated locations." This is hard to hand-code because correlations depend on terrain, weather, subject behavior—but learnable from historical data.

### Geometric Relationships

The attention mechanism in transformer-based models (Kool et al.) computes pairwise relationships between all locations. It learns: "if I visit cell A, cell B becomes less valuable (already covered)" and "cells along this ridge should be visited together (shared sweep path)." The $O(n^2)$ attention captures global structure that greedy heuristics miss.

### Constraint Satisfaction Patterns

Fuel constraints create subtle dependencies. A greedy approach might grab the highest-probability cell first, then find it can't return to base. A trained network learns: "this cell is high-value but too far—skip it, or visit it only as part of a longer loop that ends at base." This requires multi-step lookahead that's expensive to compute but cheap to learn.

### Diminishing Returns

Search has diminishing returns: once you've covered a cell, additional coverage adds little. The network learns to spread effort rather than over-search high-probability areas. The exponential POD formula means the optimal strategy isn't "search the best cell thoroughly"—it's "search many cells adequately." This non-obvious tradeoff is learnable.

## 2.3 Why Attention Mechanisms Specifically

The attention-based routing architecture (Kool et al., ICLR 2019) has become dominant for learned VRP. Here's why it works for spatial problems:

1. **Permutation invariance:** The order of input nodes shouldn't matter—only their positions and values. Attention is naturally permutation-equivariant, unlike RNNs or CNNs that impose artificial ordering.

2. **Quadratic interaction modeling:** Attention computes all pairwise interactions between nodes. For routing, this is exactly what you need—the decision to visit node A depends on all other nodes' positions, values, and current tour state.

3. **Variable-size input:** Attention handles variable numbers of nodes naturally. The same trained network can process 50-node and 500-node instances.

4. **Interpretable queries:** The attention weights reveal what the network is "looking at" when making decisions. This aids debugging and builds trust.

## 2.4 Evidence from Related Domains

Neural combinatorial optimization has demonstrated success on problems structurally similar to desert search:

- **TSP:** Kool et al. achieve within 0.1% of optimal on instances up to 100 nodes, with 10× faster inference than traditional solvers. Optimal is computable via Concorde for comparison.

- **CVRP:** Capacitated VRP results from same architecture match or beat LKH3 (state-of-art metaheuristic) on benchmark instances, with much faster inference.

- **Orienteering:** The prize-collecting variant (visit high-value nodes under budget) is directly applicable to probability-weighted search. Neural approaches outperform greedy by 5-15%.

- **Multi-vehicle:** Work by Hottung & Tierney, among others, shows neural construction + ALNS refinement outperforms pure metaheuristics on split delivery and heterogeneous fleet problems.

---

# Part III: Why Metaheuristics for Refinement

Neural networks alone have limitations. The hybrid architecture—neural construction followed by metaheuristic refinement—addresses these systematically.

## 3.1 Neural Network Limitations

### Constraint Satisfaction

Neural networks output continuous values or probability distributions. Converting these to feasible discrete solutions isn't trivial. A network might assign 0.6 probability to visiting cell A and 0.5 to cell B, but if fuel only allows one, you must choose. Greedy argmax selection can violate constraints; constrained decoding is complex.

### Local Optimality

Autoregressive construction (selecting nodes one at a time) can get stuck. Once node A is selected, node B might become optimal, but you can't undo A. The network's training minimizes expected loss, not worst-case loss—unlucky sampling can produce poor solutions.

### Distribution Shift

Networks generalize best when test instances resemble training instances. A network trained on uniformly random probability maps may struggle with real SAR distributions (multi-modal, terrain-correlated). Metaheuristics don't rely on distributional assumptions.

## 3.2 What Metaheuristics Add

### Feasibility Repair

ALNS operators can fix constraint violations. If the neural solution exceeds fuel, a "remove-and-reinsert" operator drops the most expensive detour. The solution remains feasible throughout refinement.

### Local Search

Classic operators—2-opt (uncross edges), Or-opt (relocate subsequences), swap (exchange nodes between routes)—explore the neighborhood of the neural solution. These moves are cheap to evaluate and often find 1-5% improvements.

### Escape from Local Optima

ALNS (Adaptive Large Neighborhood Search) combines constructive and destructive moves. "Destroy" operators remove portions of the solution; "repair" operators rebuild differently. This large-scale perturbation escapes basins that local search can't leave.

### Anytime Behavior

Metaheuristics improve monotonically—longer runtime means better solutions. If you have 10 seconds, you get a good solution; if you have 10 minutes, you get a better one. Neural inference is fixed-time; you can't trade compute for quality.

## 3.3 The Synergy: Warm Start + Refinement

The combination is more than the sum of parts:

1. **Better starting point:** ALNS quality depends heavily on initialization. A neural warm start puts you in a "good basin"—local search finds the local optimum quickly. Random initialization often lands in poor basins.

2. **Reduced refinement time:** Starting near-optimal means fewer iterations to converge. Neural + 100 ALNS iterations often beats random + 1000 iterations.

3. **Complementary strengths:** Neural networks are good at global structure (which regions matter, rough routing). Metaheuristics are good at local structure (exact node ordering, constraint satisfaction). Together: globally sensible, locally optimal.

## 3.4 Empirical Evidence for the Hybrid

Papers demonstrating the hybrid's effectiveness:

- **Hottung & Tierney (2020):** "Neural Large Neighborhood Search" combines learned destroy operators with hand-crafted repair. Outperforms both pure RL and pure ALNS on CVRP.

- **Chen & Tian (2019):** "Learning to Perform Local Rewriting" uses RL to select which ALNS operators to apply. Learns problem-specific search strategies.

- **Lu et al. (2020):** "Learning-based Iterative Method" embeds neural networks within iterative refinement. Shows consistent gains over standalone approaches.

---

# Part IV: Application to Desert Search

## 4.1 Problem Characteristics Favoring Neural Approaches

### Repeated Similar Instances

SAR teams operate in consistent regions (specific deserts, parks, wilderness areas). Today's search in Joshua Tree resembles last month's search in Joshua Tree. The terrain, access points, and subject behavior patterns repeat. This makes amortization worthwhile—train once on historical missions, deploy fast inference on new missions.

### Time-Critical Decisions

Search effectiveness drops rapidly with time ("probability of survival" curves). A 10-second neural inference + 1-minute ALNS refinement can deploy resources before a 10-minute pure metaheuristic finishes. Fast approximate solutions save lives.

### Complex Probability Distributions

$P(x,y)$ isn't uniform—it reflects terrain (hikers follow trails, avoid cliffs), subject profile (age, experience, equipment), weather (shelter-seeking behavior), and time (diffusion from last known point). These factors interact in ways hard to code but learnable from data. Statistical models like ISRID (International Search and Rescue Incident Database) provide training data.

## 4.2 Problem Characteristics Requiring Metaheuristics

### Hard Constraints

Fuel limits are non-negotiable. If a drone's battery is 90 minutes, it cannot fly 91 minutes. Neural outputs may violate this (probability-weighted selections don't know about fuel). ALNS enforces feasibility by removing nodes until constraints are met.

### Coverage Guarantees

SAR may require 100% POA (probability of area) coverage—every cell must be visited. Neural selection might skip low-probability cells. ALNS can enforce: "insert all unvisited cells" as a repair operator.

### Robustness to Model Mismatch

If $P(x,y)$ is wrong (bad intelligence, subject moved), the neural solution based on $P$ may be poor. ALNS refinement with actual reward feedback (cells already searched return zero value) adapts in real-time.

## 4.3 The Multi-Vehicle No-Communication Case

This is where learned approaches likely provide the most value. The problem: multiple vehicles plan simultaneously, cannot communicate during execution, must robustly cover the area even if some vehicles fail.

**Why it's hard:** Hand-coding robust multi-agent coordination is extremely difficult. How much should vehicle A's plan overlap with vehicle B's plan? It depends on B's failure probability, A's efficiency loss from overlap, and the importance of the overlapped region. These tradeoffs interact combinatorially.

**Why learning helps:** A neural network trained on scenarios with various failure rates learns implicit coordination strategies. It discovers: "vehicle A should cover high-probability regions redundantly with vehicle B, but can skip low-probability redundancy." This is exactly the kind of non-obvious tradeoff that's hard to engineer but easy to learn.

**Training approach:** Sample failure scenarios during training. Reward = POD achieved by surviving vehicles. Policy gradient methods (PPO) optimize expected reward over failure distribution. The learned policy is implicitly robust.

---

# Part V: When This Approach Won't Work

## 5.1 Small Problems

If your search region discretizes to <50 cells, exact solvers (CPLEX, Gurobi) may find optimal solutions in seconds. Neural training overhead isn't justified. Rule of thumb: if brute-force enumeration is feasible, use it.

## 5.2 Unique Instances

If you solve each problem exactly once (unique terrain, never repeated), training cost can't amortize. The neural network learns patterns across instances—with one instance, there are no patterns to learn. Use ALNS with simple initialization (nearest-neighbor, greedy-by-probability) instead.

## 5.3 Obvious Optimal Strategies

Sometimes the problem has trivial structure. If $P(x,y)$ is unimodal (single peak), the optimal strategy is obvious: start at the peak, spiral outward. A neural network that learns this has learned nothing you couldn't code in 10 lines.

**Test:** Implement greedy-by-probability (sort cells by $P(x)/\text{distance}$, visit in order). If this achieves >90% of optimal POD, the neural network's value-add is marginal. Only invest in learning if greedy leaves significant room for improvement.

## 5.4 Insufficient Training Data

Neural networks need data. For SAR, this means historical missions with known outcomes (found/not found, where found). If your organization has 10 historical missions, you can't train a robust model. You need hundreds to thousands of instances. Synthetic data (simulated $P$ distributions) helps but may not capture real-world correlations.

## 5.5 Extreme Distribution Shift

A network trained on desert search may fail spectacularly on maritime search—different physics (drift vs. walking), different $P$ distributions, different constraints. Transfer learning helps but isn't magic. Validate on held-out data from your target domain before deploying.

## 5.6 Safety-Critical Requirements

Neural networks are black boxes. If you need to explain why vehicle A was sent to location X, the attention weights provide some interpretability but not formal guarantees. If regulatory or legal requirements demand provable optimality or explainable decisions, stick with classical optimization (which is also not guaranteed optimal for NP-hard problems, but at least has understood failure modes).

---

# Part VI: Concrete Research Directions

## 6.1 Direction A: Probability-Weighted Orienteering

**Goal:** Single vehicle, maximize POD under fuel constraint.

**Why neural helps:** The reward function (POD) has complex structure: coverage of high-P cells matters more, but diminishing returns mean spreading effort beats concentration. The optimal tradeoff between exploitation (high-P cells) and exploration (coverage) is learnable.

**Architecture:** Attention encoder (Kool-style) processes cell features (P value, distance from base, terrain). Decoder autoregressively selects cells. Fuel constraint enforced by masking infeasible cells.

**Training:** REINFORCE with baseline. Reward = POD achieved. Generate synthetic P distributions (Gaussian mixtures, terrain-correlated noise) for training.

## 6.2 Direction B: Multi-Vehicle Coordination Without Communication

**Goal:** Multiple vehicles plan jointly before deployment, execute without communication, robustly cover area despite failures.

**Why neural helps:** Coordination requires reasoning about other agents' likely behavior and failures. Multi-agent RL naturally handles this—each agent learns a policy that's robust to others' actions (or inactions due to failure).

**Architecture:** Graph attention network with vehicle nodes and cell nodes. Vehicles attend to cells (what to cover) and to each other (coordination). Shared encoder, per-vehicle decoders.

**Training:** PPO with centralized critic (sees all vehicles during training), decentralized actors (each vehicle policy independent). Sample failure scenarios (vehicle k fails with probability $p_k$) during training. Reward = POD achieved by surviving vehicles.

## 6.3 Direction C: Heterogeneous Fleet Assignment

**Goal:** Assign regions to vehicles with different capabilities (speed, sweep width, fuel, sensor type).

**Why neural helps:** Assignment depends on matching vehicle capabilities to region characteristics. Drones cover rough terrain; ground vehicles cover roads faster. These matchings are learnable.

**Architecture:** Two-stage: (1) Encoder embeds vehicles and regions. (2) Bipartite attention assigns regions to vehicles. Then per-vehicle routing (Direction A) within assigned regions.

**Training:** Hierarchical RL. Upper level learns assignment; lower level learns routing. Or joint training with assignment as latent variable.

## 6.4 Direction D: Real-Time Replanning

**Goal:** Update plan as search progresses (cells cleared, clues found, P distribution updated).

**Why neural helps:** Replanning must be fast (seconds, not minutes). A trained network gives instant re-inference. Online ALNS can refine incrementally.

**Architecture:** Recurrent or Transformer with state encoding: current coverage, updated P, remaining fuel, time elapsed. Outputs continuation of tour from current position.

**Training:** Imitation learning from rolled-out optimal policies, or RL with simulated environment that updates P based on search results (Bayesian update: if not found, P in searched area drops).

---

# Part VII: Neural Generation of Geometric Search Regions

## 7.1 The Core Challenge

We want a neural network that takes a probability map $P(x,y)$ over a search region $A$ and outputs a set of geometric shapes $\{R_1, R_2, \ldots, R_k\}$ that partition or tile $A$. The key constraints that make this hard:

1. **Geometry generation:** The network outputs continuous shape parameters (vertices, centers, angles), not discrete selections from a pre-existing set of nodes.

2. **Coverage guarantee:** The union $\cup R_k$ must equal $A$ exactly (or within tolerance). No gaps allowed—this is a hard constraint, not a soft penalty.

3. **No inference-time objective:** Once trained, the network produces shapes via a single forward pass (or iterative sampling). No reward function is evaluated; no optimization loop runs. The network has internalized what "good" means.

4. **Variable output cardinality:** The number of shapes $K$ is not fixed—it depends on the input probability map's complexity.

## 7.2 Mathematical Setup

**Input:** Probability density $P: A \to [0,1]$ where $A \subset \mathbb{R}^2$ is the search region (e.g., 100×100 km polygon). $P$ represents the likelihood distribution of a search target. May be discretized as an $H \times W$ image.

**Output:** A finite set $S = \{R_1, \ldots, R_k\}$ of convex shapes (rectangles, parallelograms, or convex polygons). Each shape $R_i$ is parameterized by $\theta_i \in \mathbb{R}^d$ (e.g., $d=4$ for axis-aligned rectangles: center_x, center_y, width, height).

**Coverage constraint:** $\cup_i R_i \supseteq A$ (complete coverage). Ideally also $\cap_{i \neq j}(R_i \cap R_j)$ has small measure (minimal overlap).

**Implicit objective:** Shapes should be ordered and sized so that searching them sequentially maximizes expected probability of detection (POD) under time/fuel constraints. But this objective is only used during training—not at inference.

## 7.3 Why Standard Approaches Fail

- **Standard VRP:** Assumes nodes exist. Attention-based routing (Kool et al.) selects from N given cities. Here, we must generate the nodes themselves.

- **Object detection (YOLO/Faster-RCNN):** Outputs bounding boxes, but doesn't guarantee coverage or non-overlap. Designed to find existing objects, not tile space.

- **Semantic segmentation:** Outputs pixel-wise labels. Converting to geometric shapes requires post-processing that may not produce valid tilings.

- **Reinforcement learning:** Typically requires reward evaluation at inference (Q-values, policy gradients). We want the policy to execute blindly.

---

# Part VIII: Shape Parameterization and Representation

The choice of shape parameterization fundamentally constrains what the network can learn and how coverage is enforced. Each option trades flexibility against tractability.

## 8.1 Axis-Aligned Rectangles (4 parameters)

**Parameterization:** $\theta = (c_x, c_y, w, h)$ where $(c_x, c_y)$ is center, $w$ is width, $h$ is height. All values normalized to $[0,1]$ relative to bounding box of $A$.

**Conversion to vertices:** $V = \{(c_x \pm w/2, c_y \pm h/2)\}$. Four corners computed directly.

**Intersection test:** Two rectangles $R_1, R_2$ intersect iff their x-ranges overlap AND their y-ranges overlap. $O(1)$ per pair.

**Coverage verification:** Rasterize to grid, compute union mask, check mask covers $A$. $O(HW)$ for $H \times W$ grid. Differentiable via soft rasterization.

**Pros:** Simplest intersection/union math. Lawnmower search pattern trivial within each rectangle. Network output is directly interpretable.

**Cons:** Cannot align with diagonal probability contours or terrain features. May require many small shapes to approximate curves. Inefficient when optimal sweep direction isn't axis-aligned.

## 8.2 Rotated Rectangles (5 parameters)

**Parameterization:** $\theta = (c_x, c_y, w, h, \phi)$ where $\phi \in [0, \pi)$ is rotation angle. (Symmetry: $\phi$ and $\phi + \pi$ give same rectangle.)

**Conversion to vertices:** Apply rotation matrix $R(\phi)$ to axis-aligned corners, then translate by $(c_x, c_y)$.

**Intersection test:** Separating Axis Theorem (SAT). For two convex polygons, they don't intersect iff there exists a separating axis (perpendicular to some edge). For rectangles: test 4 edge normals. $O(1)$ per pair but ~4× more expensive than axis-aligned.

**Coverage verification:** Same rasterization approach, but rotation requires interpolation. Or: compute polygon union analytically (Sutherland-Hodgman clipping), then check area.

**Pros:** Can align sweep direction with terrain (ridges, roads) or probability contours. More efficient coverage with fewer shapes when features are diagonal.

**Cons:** Angle prediction can be tricky (wrap-around at π). Intersection/union more complex. Lawnmower path within rotated rectangle requires coordinate transform.

## 8.3 General Convex Polygons (Variable parameters)

**Parameterization options:**

- Vertex sequence: $\theta = \{(x_1,y_1), (x_2,y_2), \ldots, (x_n,y_n)\}$. Variable $n$ requires handling (pad to max, or autoregressive).

- Polar form: Center $(c_x,c_y)$ + radii $r_1 \ldots r_n$ at fixed angles. Guarantees star-convexity from center.

- Support function: For convex set $C$, $h_C(u) = \max\{\langle u,x \rangle : x \in C\}$. Discretize over unit circle directions. Always produces convex shape.

**Convexity enforcement:** If using raw vertices, must either: (a) project onto convex hull post-hoc (not differentiable), (b) use convex combination of fixed templates, or (c) parameterize as above.

**Pros:** Maximum flexibility. Can match any convex contour.

**Cons:** Variable vertex count complicates batching. Intersection is $O(n+m)$ polygon clipping. Coverage is harder to verify. Lawnmower pattern less obvious inside irregular polygon.

## 8.4 Strip Decomposition (Coverage Guaranteed by Construction)

This is a restricted but powerful representation where coverage is guaranteed structurally.

**Parameterization:** $\theta = (\phi, \{w_1, w_2, \ldots, w_n\})$ where $\phi$ is the sweep angle and $w_i$ are strip widths perpendicular to angle $\phi$. The strips are parallel bands that span the entire region $A$ in direction $\phi$.

**Coverage guarantee:** If $\sum w_i = W_{\text{total}}$ (the extent of $A$ perpendicular to $\phi$), coverage is automatic. Enforce via softmax: $w_i = \text{softmax}(\text{logits})_i \times W_{\text{total}}$. The strips perfectly tile $A$ by construction—no gaps possible.

**What the network learns:** (1) Optimal sweep angle $\phi$—align strips with probability contours to get high-P regions in fewer strips. (2) Variable strip widths—narrow strips in high-P regions (thorough search), wide strips in low-P regions (fast coverage).

**Pros:** Coverage is mathematically guaranteed. Simple lawnmower within each strip. Angle learning is meaningful (terrain alignment). Softmax trick makes the whole thing differentiable.

**Cons:** All strips share same angle (can't have perpendicular strips in different parts). For complex $P(x,y)$ with multiple modes at different orientations, may need hierarchical decomposition.

**Recommendation:** Start with strip decomposition. It dramatically simplifies the problem while still allowing meaningful learning. Graduate to rotated rectangles only after validating the pipeline.

---

# Part IX: Architecture A — DETR-Style Set Prediction

DETR (Detection Transformer, Carion et al. 2020) revolutionized object detection by treating it as a direct set prediction problem. We adapt this architecture to generate search regions.

## 9.1 DETR Fundamentals

**Core idea:** Instead of anchor boxes + NMS post-processing, use N learnable "object queries" that each attend to the image and output one bounding box. Training uses Hungarian matching to assign predictions to ground truth.

**Why it matters for us:** (1) Outputs a set of shapes in parallel—no sequential dependencies. (2) Self-attention between queries lets them coordinate to avoid overlap. (3) Cross-attention to image features lets queries focus on relevant regions of $P(x,y)$. (4) No objective function at inference—just forward pass.

## 9.2 Architecture Detail

### Encoder (Image → Feature Map)

1. **Backbone CNN:** ResNet-50 or U-Net processes $P(x,y)$ as a single-channel (or 3-channel with copies) image. Output: feature map $F \in \mathbb{R}^{C \times H' \times W'}$ where $H'=H/32$, $W'=W/32$, $C=2048$ for ResNet.

2. **Dimensionality reduction:** 1×1 convolution reduces $C \to d$ (e.g., $d=256$). Now $F \in \mathbb{R}^{d \times H' \times W'}$.

3. **Flatten + positional encoding:** Reshape $F$ to sequence of $H'W'$ tokens, each $\in \mathbb{R}^d$. Add 2D sinusoidal positional encodings so transformer knows spatial locations.

4. **Transformer encoder:** $L$ layers of multi-head self-attention + FFN. Typical: $L=6$, heads=8. Output: contextualized features $Z \in \mathbb{R}^{H'W' \times d}$.

### Decoder (Object Queries → Shapes)

1. **Object queries:** $N$ learnable embedding vectors $Q \in \mathbb{R}^{N \times d}$. These are the "slots" that will become shape predictions. $N$ should exceed expected max shapes (e.g., $N=50$ for typical decompositions of 5-20 shapes).

2. **Transformer decoder:** $L'$ layers (typically $L'=6$). Each layer has: (a) Self-attention over queries—lets queries coordinate, avoid proposing overlapping shapes. (b) Cross-attention from queries to $Z$—lets each query attend to relevant parts of $P(x,y)$. (c) FFN for feature transformation.

3. **Output:** Transformed queries $O \in \mathbb{R}^{N \times d}$, one per slot.

### Prediction Heads

Each of the $N$ output vectors is fed to prediction heads:

- **Shape head:** MLP (e.g., 2 hidden layers, 256 units, ReLU) → $\mathbb{R}^5$ for rotated rectangle $(c_x, c_y, w, h, \phi)$. Use sigmoid to normalize $c_x, c_y, w, h$ to $[0,1]$; $\tanh \times \pi/2$ for $\phi \in [-\pi/2, \pi/2]$.

- **Existence head:** MLP → $\mathbb{R}^1$ → sigmoid. Probability that this slot contains a real shape vs. "no-object". During inference, filter slots where existence_prob > τ (e.g., τ=0.7).

- **Priority head (optional):** MLP → $\mathbb{R}^1$. Scalar indicating search priority (higher = search first). Alternatively, derive priority from order in which queries are matched during training.

## 9.3 Training with Hungarian Matching

The key insight of DETR is using bipartite matching to handle set prediction. Given a prediction set $\{\hat{y}_1, \ldots, \hat{y}_N\}$ and ground truth $\{y_1, \ldots, y_M\}$ (padded to $N$ with "no-object" labels):

1. **Compute cost matrix:** $C[i,j] = \text{cost of assigning prediction } i \text{ to ground truth } j$. Cost = $\lambda_{\text{cls}} \times L_{\text{cls}}(p_i, c_j) + \lambda_{\text{box}} \times L_{\text{box}}(b_i, b_j)$ where $L_{\text{cls}}$ is cross-entropy for existence/no-object, $L_{\text{box}}$ is L1 + GIoU loss for shape parameters.

2. **Hungarian algorithm:** Find optimal bijection $\sigma: \{1,\ldots,N\} \to \{1,\ldots,N\}$ minimizing $\sum_i C[i, \sigma(i)]$. This is $O(N^3)$ but $N$ is small (~50).

3. **Compute loss:** $L = \sum_i [\lambda_{\text{cls}} \times L_{\text{cls}}(p_i, c_{\sigma(i)}) + \mathbf{1}\{c_{\sigma(i)} \neq \emptyset\} \times (\lambda_{L1} \times L_{L1}(b_i, b_{\sigma(i)}) + \lambda_{\text{GIoU}} \times L_{\text{GIoU}}(b_i, b_{\sigma(i)}))]$. Only penalize box loss for matched real objects.

## 9.4 Ensuring Coverage

DETR doesn't natively guarantee coverage. We add auxiliary mechanisms:

- **Coverage loss during training:** $L_{\text{coverage}} = 1 - \text{Area}(\cup R_i \cap A) / \text{Area}(A)$. Computed via differentiable rasterization: render each shape as soft mask (pixel value = sigmoid of signed distance to boundary), compute element-wise max for union, compare to target mask of $A$.

- **Repair layer at inference:** After forward pass, compute gap regions $G = A \setminus (\cup R_i)$. If $G$ is non-empty, either: (a) expand nearest shape to cover $G$, or (b) generate minimal bounding rectangles for connected components of $G$. Not differentiable, but guarantees coverage.

- **Curriculum learning:** Start with high $\lambda_{\text{coverage}}$ weight, gradually reduce as network learns. Track "repair rate" (fraction of outputs needing repair); if it stays high, network isn't learning coverage.

## 9.5 Why No Objective at Inference

Once trained, the network executes a single forward pass: $P(x,y) \to$ CNN $\to$ Transformer encoder $\to$ Transformer decoder $\to$ heads $\to$ {shapes}. No loss computation, no gradient, no optimization loop. The weights encode what "good decomposition" means. This is exactly like a trained image classifier: at test time, you just run forward pass, not SGD.

---

# Part X: Architecture B — Autoregressive Polygon Generation

Instead of predicting all shapes in parallel, generate them one at a time, conditioning each new shape on the shapes already produced. This mirrors how language models generate tokens sequentially.

## 10.1 Precedent: PolyGen and Polygon-RNN

**PolyGen (Nash et al., 2020):** Generates 3D meshes by autoregressively predicting vertices, then faces. Uses Transformer decoder with masked self-attention. Vertices are quantized to discrete bins and predicted via categorical distribution (like language models predicting words).

**Polygon-RNN (Castrejon et al., 2017):** Traces object contours by sequentially predicting polygon vertices. Uses CNN encoder + RNN decoder. Each step outputs a vertex position on a discretized grid.

**PolyFormer (Liu et al., 2023):** Seq2seq Transformer that outputs polygon vertices as floating-point coordinates. Key innovation: regression-based decoder avoids quantization error. Outputs multiple polygons separated by <SEP> tokens.

## 10.2 State Representation

At each step $t$, the model sees:

1. **Probability map $P(x,y)$:** The original input, unchanged.

2. **Coverage mask $M_t$:** Binary (or soft) mask indicating regions already covered by shapes $R_1, \ldots, R_{t-1}$. The "uncovered probability" is $P(x,y) \odot (1 - M_t)$.

3. **Shape history:** Embeddings of previously generated shapes. Either: (a) concatenate shape parameter vectors and pass through encoder, or (b) use Transformer decoder where previous shape tokens form the autoregressive context.

4. **Resource budget:** Remaining fuel/time. As a scalar input to the model, or embedded and concatenated to features.

## 10.3 Network Architecture

### Option A: CNN Encoder + Transformer Decoder

- **Encoder:** U-Net or ResNet processes $[P, M_t]$ (stacked as 2 channels) → feature map $F_t$.

- **Decoder:** Transformer with causal masking. Input sequence = embeddings of shapes $1, \ldots, t-1$. Cross-attends to $F_t$. Outputs embedding for shape $t$.

- **Prediction heads:** MLP from decoder output → shape parameters $(c_x, c_y, w, h, \phi)$ + STOP probability.

### Option B: Full Transformer (Image Patches)

- **Vision Transformer style:** Divide $[P, M_t]$ into patches, embed as tokens. Concatenate with shape history tokens. Full self-attention (image patches) + causal attention (shape tokens).

- **Output:** Next shape token, decoded to parameters via MLP.

## 10.4 Output Parameterization: Discrete vs Continuous

### Discrete (PolyGen style)

Quantize each parameter to $B$ bins (e.g., $B=256$). Output is categorical distribution over bins. Advantages: can use standard cross-entropy loss, softmax sampling for diversity. Disadvantage: quantization error, very fine control requires large $B$.

**Example:** For $c_x \in [0,1]$ with $B=256$ bins: $c_x^{\text{discrete}} = \text{round}(c_x \times 255)$. Network outputs logits $\in \mathbb{R}^{256}$, train with cross-entropy against $c_x^{\text{discrete}}$.

### Continuous (Regression)

Output mean $\mu$ and optionally variance $\sigma^2$ for each parameter. Train with negative log-likelihood of Gaussian (equivalent to MSE loss when $\sigma$ is fixed). Advantages: no quantization, can model uncertainty. Disadvantage: harder to get multimodal outputs.

**Mixture Density Networks:** For multimodal distributions (e.g., shape could go in either of two locations), output parameters of Gaussian mixture: $\{(w_k, \mu_k, \Sigma_k)\}$. Sampling: choose component $k$ with prob $w_k$, then sample from $\mathcal{N}(\mu_k, \Sigma_k)$.

## 10.5 Termination Mechanism

- **STOP token:** Add a binary classification head: $P(\text{stop} | \text{state})$. Generate shapes until $P(\text{stop}) > \tau$ or hard limit reached.

- **Coverage threshold:** Continue generating until $\text{Area}(\cup R_i) / \text{Area}(A) > 0.99$. Guarantees coverage but requires computing area at each step.

- **Fixed number:** Always generate exactly $K$ shapes. Simpler but may waste computation or leave gaps.

## 10.6 Training

### Imitation Learning (Behavioral Cloning)

Requires dataset of $(P, \{R_1, \ldots, R_k\})$ pairs where the shapes are optimal decompositions (from classical solver or human expert). Training: teacher forcing. At each step $t$, provide ground-truth shapes $1..t-1$, predict shape $t$. Loss = $\sum_t L(\hat{R}_t, R_t)$.

**Issue—compounding error:** At test time, model sees its own predictions, not ground truth. Errors accumulate. Mitigation: scheduled sampling (gradually replace ground truth with model predictions during training), or DAgger (collect new training data from model's own trajectories).

### Reinforcement Learning

No ground-truth decompositions needed. Reward signal:

$$r_t = \int_{R_t \setminus \cup_{i<t} R_i} P(x,y) \, dx \, dy - \lambda \times \text{time}(R_t) - \mu \times \text{overlap}(R_t, \cup_{i<t} R_i)$$

This is: probability mass newly covered by $R_t$, minus time cost, minus overlap penalty. Terminal bonus for full coverage.

**Algorithms:** REINFORCE (high variance, simple), PPO (lower variance, more stable), or soft actor-critic (SAC) for continuous actions. Note: reward is only computed during training. At inference, just sample from policy $\pi(R_t | \text{state})$.

## 10.7 Coverage Guarantee

Autoregressive models naturally track coverage via $M_t$. Options: (1) Continue until coverage ≥ threshold (exact but requires area computation each step). (2) Train the STOP head to correlate with high coverage—network learns when to stop. (3) Post-process: after STOP, check coverage, add repair shapes if needed.

---

# Part XI: Architecture C — Diffusion Models for Shape Generation

Diffusion models have achieved remarkable results in image generation and, more recently, combinatorial optimization. They learn to reverse a noise-adding process, starting from pure noise and progressively denoising to a high-quality sample.

## 11.1 Background: Denoising Diffusion

**Forward process:** Given data $x_0$ (e.g., shape parameters), add Gaussian noise over $T$ steps: $x_t = \sqrt{\bar{\alpha}_t} x_0 + \sqrt{1-\bar{\alpha}_t} \epsilon$, where $\epsilon \sim \mathcal{N}(0,I)$ and $\bar{\alpha}_t$ is a noise schedule (decreases from 1 to ~0).

**Reverse process:** Learn neural network $\epsilon_\theta(x_t, t)$ to predict the noise $\epsilon$ that was added. Iteratively denoise: $x_{t-1} = (x_t - \sqrt{1-\bar{\alpha}_t} \epsilon_\theta(x_t, t)) / \sqrt{\bar{\alpha}_t} + \sigma_t z$, where $z \sim \mathcal{N}(0,I)$.

**Training objective:** $L = \mathbb{E}_{x_0, \epsilon, t}[\|\epsilon - \epsilon_\theta(x_t, t)\|^2]$. Simple denoising autoencoder loss at randomly sampled noise levels.

**Inference:** Sample $x_T \sim \mathcal{N}(0,I)$, iteratively apply reverse process to get $x_0$. No objective function evaluation—just forward passes of $\epsilon_\theta$.

## 11.2 DIFUSCO: Diffusion for Combinatorial Optimization

DIFUSCO (Sun et al., NeurIPS 2023) applies diffusion to TSP and MIS problems. Key adaptations:

1. **Binary representation:** For TSP, represent solution as edge indicator vector $x \in \{0,1\}^{n^2}$. For us: represent shape inclusion as binary vector over discretized shape space.

2. **Discrete diffusion:** Instead of Gaussian noise, use Bernoulli noise (bit flips). Forward process randomly flips bits; reverse process learns to un-flip. Outperforms continuous diffusion for discrete problems.

3. **Graph neural network backbone:** $\epsilon_\theta$ is an Anisotropic GNN that processes problem structure (cities, edges). For us: process probability map $P$ as graph or image.

4. **Greedy decoding + 2-opt:** After diffusion, apply greedy decoding to convert soft outputs to valid solution, then local search refinement.

## 11.3 Adaptation for Shape Generation

### Representation

Flatten $K$ shapes into vector $x \in \mathbb{R}^{K \times d}$ where $d=5$ (rotated rectangle parameters). Pad to fixed $K_{\max}$. The "data distribution" is the set of valid decompositions for inputs $P$.

### Conditional Generation

Must condition on probability map $P$. Options:

- **Concatenation:** Flatten $P$ to vector, concatenate with $x_t$, feed to denoiser. Simple but doesn't exploit spatial structure.

- **Cross-attention:** Encode $P$ via CNN → feature map. Denoiser attends to these features. Maintains spatial structure, allows shapes to "look at" relevant regions of $P$.

- **FiLM conditioning:** Compute global features from $P$, use them to modulate denoiser's batch norm (scale and shift). Lightweight but less expressive.

### Denoiser Architecture

Since shapes are a small set of vectors (not high-dimensional images), use MLP or small Transformer as denoiser. Input: $[x_t, t_{\text{embedding}}, P_{\text{features}}]$. Output: predicted noise $\hat{\epsilon} \in \mathbb{R}^{K_{\max} \times d}$. Or predict $x_0$ directly ("$x_0$ parameterization").

## 11.4 Unsupervised Diffusion (Sanokowski et al., ICML 2024)

Standard diffusion requires training data (optimal decompositions). Sanokowski et al. show how to train without labels:

- **Key idea:** Instead of matching data distribution, minimize an energy function $E(x)$ over generated samples. Use a loss that upper-bounds reverse KL divergence: $L \approx \mathbb{E}_{x \sim q_\theta}[E(x)] + \text{entropy}(q_\theta)$.

- **For us:** $E(\text{shapes}) = -\int P(x,y) \mathbf{1}_{\text{covered}}(x,y) \, dx \, dy + \lambda \times T + \mu \times \text{overlap}$. During training, evaluate this energy. At inference, just sample—no energy evaluation needed.

- **Advantage:** No need to compute optimal decompositions as labels. The diffusion model learns to sample from low-energy (high-quality) configurations.

## 11.5 Ensuring Coverage

- **Training:** Include coverage term in energy: $E += \lambda_{\text{cov}} \times (1 - \text{coverage\_fraction})$. Model learns to generate covering tilings.

- **Guidance:** Classifier-free guidance variant: train both conditional (given $P$) and unconditional models, interpolate at inference to strengthen conditioning.

- **Projection:** After each denoising step, project $x_t$ onto feasible set (shapes that cover $A$). Expensive but guarantees feasibility. For strip decomposition, projection is just normalizing widths to sum correctly.

## 11.6 Inference-Time Behavior

At inference: sample $x_T \sim \mathcal{N}(0,I)$, run $T$ denoising steps, decode to shapes. No energy function, no reward, no optimization. The reverse process is deterministic given $\epsilon_\theta$ (up to injected noise). Total compute: $T$ forward passes × denoiser cost. Typically $T=50$-1000 steps, but accelerated samplers (DDIM) can reduce to ~20.

---

# Part XII: Architecture D — Strip Decomposition Network

This is the simplest architecture that guarantees coverage by construction.

## 12.1 Problem Reduction

Constrain shapes to be parallel strips at a single angle $\phi$. The network learns: (1) the optimal angle $\phi \in [0, \pi)$, and (2) the widths $\{w_1, \ldots, w_n\}$ of $n$ strips perpendicular to $\phi$.

**Why this helps:** Coverage is guaranteed if $\sum w_i$ equals the extent of $A$ perpendicular to $\phi$. We enforce this via softmax normalization. No gap is possible because strips are adjacent by definition.

## 12.2 Architecture

1. **Encoder:** CNN (ResNet-18 or U-Net) processes $P(x,y)$ → feature vector $f \in \mathbb{R}^{512}$.

2. **Angle head:** MLP: $f \to \mathbb{R}^2 \to \text{atan2} \to \phi \in [0, \pi)$. Output 2D vector $(\cos, \sin)$, take atan2 for angle. Avoids discontinuity at $0/\pi$.

3. **Width head:** MLP: $f \to \mathbb{R}^n$ (logits) $\to$ softmax $\to w \in \Delta^n$ (simplex). Multiply by $W_{\text{total}}(\phi)$ to get actual widths. $W_{\text{total}}$ depends on $\phi$ because rotating the sweep direction changes the perpendicular extent.

4. **Output:** $(\phi, \{w_1, \ldots, w_n\})$. Convert to strip geometries: each strip is a rectangle with width $w_i$, length = extent of $A$ along $\phi$, positioned at cumulative offset $\sum_{j<i} w_j$ from one edge.

## 12.3 Training

### Supervised

Generate synthetic probability maps $P$ (Gaussian mixtures, random modes). For each, compute optimal $(\phi^*, w^*)$ via grid search or gradient descent. Loss = $L_{\text{angle}}(\phi, \phi^*) + L_{\text{width}}(w, w^*)$ where $L_{\text{angle}} = 1 - \cos(\phi - \phi^*)$ (invariant to 180° shifts), $L_{\text{width}}$ = KL divergence or MSE on simplex.

### Reinforcement Learning

Reward = expected POD under search strategy that visits strips in probability-weighted order, with time proportional to strip area / (speed × sweep_width). Train with PPO: policy outputs $(\phi, w)$, environment returns reward, no reward at inference.

### Self-Supervised

No labels needed. Define differentiable objective: $J(\phi, w; P) = \sum_i (\int_{\text{strip}_i} P \, dx \, dy)^2 / \text{Area}(\text{strip}_i)$. This rewards narrow strips in high-density regions (more probability per unit area). Maximize $J$ directly via gradient ascent on network parameters.

## 12.4 Implementation Details

- **Number of strips $n$:** Fixed hyperparameter. Start with $n=10$. Too few = can't capture fine structure. Too many = overfitting to noise in $P$.

- **$W_{\text{total}}$ computation:** For convex polygon $A$, $W_{\text{total}}(\phi) = \max_{x \in A} (x \cdot n_\phi) - \min_{x \in A} (x \cdot n_\phi)$ where $n_\phi = (-\sin \phi, \cos \phi)$ is perpendicular to sweep direction. Precompute for discretized $\phi$ values, interpolate.

- **Differentiable integration:** To compute $\int_{\text{strip}_i} P \, dx \, dy$ differentiably, use Monte Carlo: sample points uniformly in strip, multiply by $P(\text{point})$, average and scale by area. Or use grid summation if $P$ is discretized.

---

# Part XIII: Training Without Inference-Time Objectives

This section addresses the core requirement: the network must not evaluate any objective function at inference.

## 13.1 The Fundamental Principle

Training and inference are fundamentally different:

- **Training:** Network parameters $\theta$ are adjusted to minimize loss $L(\theta)$. This requires computing $L$, computing gradients $\nabla_\theta L$, and updating $\theta$. Slow, requires backprop, needs labels or rewards.

- **Inference:** Given fixed $\theta$, compute $f_\theta(\text{input})$ via forward pass. No loss, no gradient, no update. Fast, just matrix multiplications and nonlinearities.

The key insight: once the network is trained, it has "compiled" the objective into its weights. The objective no longer needs to be evaluated—the network directly produces outputs that (hopefully) optimize it.

## 13.2 Imitation Learning

**Training:** Given dataset $\{(P_i, S_i^*)\}$ where $S_i^*$ is the optimal decomposition for probability map $P_i$ (computed offline by any method), train network to predict $S_i^*$ from $P_i$. Loss = matching loss between predicted and ground-truth shapes.

**Inference:** Forward pass: $P \to$ network $\to S$. The optimal decompositions $S_i^*$ were computed using an objective (POD, coverage, time). But this objective is never evaluated during inference—the network learned to approximate the mapping $P \to S^*$ directly.

**Generating training data:** Use MILP solver, metaheuristic (genetic algorithm, simulated annealing), or exhaustive search on small problems. Computational cost paid once offline. Network distills this into fast forward pass.

## 13.3 Reinforcement Learning

**Training:** Policy $\pi_\theta(\text{action} | \text{state})$ produces decompositions. Environment returns reward $R$ (based on POD, coverage, time). Gradient: $\nabla_\theta J = \mathbb{E}[\nabla_\theta \log \pi_\theta(a|s) \times R]$. Update $\theta$ via SGD.

**Inference:** Sample from $\pi_\theta(\cdot|P)$ or take mode/mean. No reward computation. The policy learned which actions lead to high reward; at test time it simply executes those actions.

**Analogy:** A chess engine trained via RL doesn't compute "am I winning?" at each move—it just outputs the move its policy says is best. The value function (objective) was used during training; at play time, only the policy runs.

## 13.4 Energy-Based Models / Diffusion

**Training:** Learn energy function $E_\theta(x)$ such that low-energy configurations are good decompositions. Or equivalently, learn score function $\nabla_x \log p(x)$ where $p(x) \propto \exp(-E(x))$. Loss involves computing $E$ on samples.

**Inference:** Run Langevin dynamics or diffusion sampling to get low-energy samples. This involves evaluating $\nabla_x E_\theta(x)$ (the score), which is a forward pass through the score network—not the original energy/objective. For diffusion: run denoising steps using learned denoiser $\epsilon_\theta(x_t, t)$.

**Key distinction:** $E(x)$ is the objective (POD, coverage). $E_\theta(x)$ or $\epsilon_\theta(x_t, t)$ is the learned approximation. At inference, only the learned network runs—not the original objective formula.

## 13.5 Self-Supervised / Contrastive

**Training:** Generate positive pairs $(P, \text{good decomposition})$ and negative pairs $(P, \text{bad decomposition})$. Train encoder to score positives higher than negatives. Or: train generator $G_\theta$ to fool discriminator $D$ that tries to distinguish good vs bad decompositions.

**Inference:** Run $G_\theta(P) \to$ decomposition. No discriminator, no scoring. Generator learned to produce decompositions that "look good" according to whatever criterion distinguished positives from negatives.

---

# Section A — Geometry Representations for Coverage-Guaranteed Search

## A1. Why Geometry Is the Core Difficulty

Routing, scheduling, and allocation are *secondary* difficulties. The **primary difficulty** in desert / wilderness search is that the decision variables live in **continuous space**.

Unlike classical VRP:

- nodes are **not given**
- coverage matters, not visitation
- partial coverage has diminishing returns
- geometry determines both feasibility *and* efficiency

If geometry is chosen poorly:

- routing explodes combinatorially
- constraints become brittle
- neural models overfit discretization artifacts
- classical solvers waste effort fixing upstream mistakes

Therefore, geometry must be treated as a **first-class design object**, not a preprocessing step.

## A2. Formal Geometric Requirements

Let:

- $A \subset \mathbb{R}^2$ be the search domain (polygonal, possibly non-convex)
- $P : A \to [0,1]$ be a probability density
- $\{R_i\}_{i=1}^K$ be generated regions

We require:

### A2.1 Coverage

Hard requirement:
$$\bigcup_{i=1}^K R_i \supseteq A$$

Soft variants (used only in training):
$$\frac{\text{Area}\left(\bigcup_i R_i \cap A\right)}{\text{Area}(A)} \ge \alpha \quad (\alpha \approx 0.99)$$

### A2.2 Searchability

Each $R_i$ must admit a **constructive coverage path**:

- lawnmower / boustrophedon
- bounded curvature
- computable length

This immediately excludes arbitrary shapes unless carefully parameterized.

### A2.3 Compositionality

Regions must:

- be reorderable
- be assignable to vehicles
- support merging / splitting during repair

## A3. Geometry Representation Design Space

We now enumerate the **full representation space**, with explicit tradeoffs.

## A4. Axis-Aligned Rectangles

### A4.1 Parameterization

Each rectangle:
$$\theta = (c_x, c_y, w, h)$$

Vertices:
$$(c_x \pm w/2, c_y \pm h/2)$$

### A4.2 Properties

**Advantages**

- trivial intersection tests
- trivial rasterization
- trivial sweep path
- numerically stable

**Disadvantages**

- cannot align with terrain
- inefficient for diagonal probability contours
- often requires many small rectangles

### A4.3 When They Make Sense

- debugging
- sanity baselines
- ablation studies
- environments with grid-aligned structure

Axis-aligned rectangles are *foundational but insufficient*.

## A5. Rotated Rectangles

### A5.1 Parameterization

$$\theta = (c_x, c_y, w, h, \phi) \quad \phi \in [0,\pi)$$

Rotation matrix:
$$R(\phi) = \begin{pmatrix} \cos \phi & -\sin \phi \\ \sin \phi & \cos \phi \end{pmatrix}$$

### A5.2 Geometry

Vertices obtained by:
$$v_k = R(\phi) v_k^{\text{axis}} + (c_x, c_y)$$

Intersection test:

- Separating Axis Theorem (4 axes)
- $O(1)$, but with larger constant

### A5.3 Search Path

Transform to local coordinates:

- sweep along long axis
- transform back

### A5.4 Failure Modes

- angle wraparound discontinuities
- small angle errors → large boundary drift
- overlapping rectangles unless explicitly penalized

Rotated rectangles are the **minimum viable generalization** beyond strips.

## A6. General Convex Polygons

### A6.1 Why Consider Them

They can approximate:

- ridges
- basins
- irregular probability contours

But representation matters enormously.

### A6.2 Raw Vertex Lists (Bad Default)

$$\theta = \{(x_1,y_1),\dots,(x_n,y_n)\}$$

Problems:

- variable length
- convexity not guaranteed
- permutation ambiguity
- unstable gradients

**Conclusion:** Do *not* use raw vertices unless post-processed.

### A6.3 Star-Convex / Polar Parameterization

$$\theta = (c_x, c_y, r_1,\dots,r_n)$$

Vertices:
$$v_k = (c_x, c_y) + r_k (\cos \alpha_k,\sin \alpha_k)$$

Properties:

- guaranteed star-convexity
- fixed dimensionality
- still expensive to intersect

### A6.4 Support Function Representation (Best Convex Form)

For directions $u_j \in S^1$:
$$h(u_j) = \max_{x \in R} \langle u_j, x \rangle$$

Defines a convex body uniquely.

Pros:

- convexity guaranteed
- stable under optimization
- elegant mathematically

Cons:

- non-intuitive
- harder to rasterize
- sweep paths non-trivial

Convex polygons are **expressive but heavy**. They are rarely the first correct choice.

## A7. Strip Decomposition (Coverage by Construction)

This is the **most important representation**.

### A7.1 Definition

Choose a global sweep angle $\phi$. Define perpendicular normal:
$$n_\phi = (-\sin\phi,\cos\phi)$$

Project region:
$$W_{\text{total}}(\phi) = \max_{x \in A} x \cdot n_\phi - \min_{x \in A} x \cdot n_\phi$$

Define strip widths $\{w_i\}_{i=1}^N$ such that:
$$\sum_i w_i = W_{\text{total}}(\phi)$$

Each strip spans entire region along sweep direction.

### A7.2 Parameterization

Neural outputs:

- angle vector $(\cos\phi, \sin\phi)$
- logits $\ell_i$

Widths:
$$w_i = \text{softmax}(\ell)_i \cdot W_{\text{total}}(\phi)$$

**Coverage is mathematically guaranteed.**

### A7.3 Geometry Construction

Strip boundaries:
$$b_0 = \min_{x\in A} x \cdot n_\phi$$
$$b_k = b_0 + \sum_{i=1}^k w_i$$

Strip $i$:
$$R_i = \{x \in A : b_{i-1} \le x \cdot n_\phi \le b_i\}$$

### A7.4 Why Strips Are Special

| Property | Value |
|----------|-------|
| Coverage | Guaranteed |
| Differentiability | Full |
| Dimensionality | Low |
| Interpretability | High |
| Failure modes | Predictable |

They impose a **strong inductive bias**:

> "Search efficiently means aligning with structure."

This is *desirable* in SAR.

## A8. Coverage Enforcement Mechanisms (General)

For representations without structural guarantees:

### A8.1 Differentiable Coverage Loss (Training Only)

Rasterize shapes → soft masks $M_i(x,y)$

Union:
$$M(x,y) = \max_i M_i(x,y)$$

Coverage loss:
$$L_{\text{cov}} = 1 - \frac{\sum M(x,y)}{\sum \mathbf{1}_A(x,y)}$$

### A8.2 Repair Operators (Inference)

Given uncovered region:
$$G = A \setminus \bigcup_i R_i$$

Options:

- expand nearest region
- add minimal bounding rectangles
- fallback strips for gaps

Repair is:

- deterministic
- auditable
- allowed (neural model is not sacred)

## A9. Geometry Choice Rules (Design Doctrine)

1. **If strips work, do not generalize**
2. **Generalize only to fix specific failures**
3. **Never give neural models unconstrained geometry**
4. **Guarantees > expressiveness**
5. **Repair beats penalty tuning**

## A10. Why Geometry Comes First (Key Insight)

Bad geometry:

- poisons learning
- bloats routing
- destabilizes constraints

Good geometry:

- simplifies everything downstream
- exposes real structure
- enables hybrid systems to work

> **This entire research program rests on the claim that geometry is learnable — and routing is easier once it is.**

---

# Section B — Training Paradigms Without Inference-Time Objectives

This section answers a single, non-negotiable requirement:

> **At inference, the system must not evaluate objectives, rewards, or energies.**

All learning pressure is applied **offline**. At runtime, the model is a *compiled geometric policy*.

## B1. Training vs Inference: A Hard Separation

Let $\theta$ be network parameters, $f_\theta$ the geometry generator.

- **Training**:
  - objectives evaluated
  - gradients computed
  - constraints may be soft

- **Inference**:
  - single forward pass
  - optional deterministic repair
  - no scoring, no optimization

This separation is **architectural**, not stylistic.

## B2. Supervised Geometry Learning (Bootstrapping)

### B2.1 Purpose

Supervised learning is **not** the final method. It is used to:

- stabilize early training
- shape inductive bias
- reduce RL variance

### B2.2 Data Generation

Training pairs:
$$(P^{(k)}, \{R_i^{*(k)}\})$$

Ground truth obtained via:

- grid search (for strip angle)
- metaheuristics (small instances)
- human-designed heuristics

Important: **Labels need not be optimal**, only *reasonable*.

### B2.3 Losses by Geometry Type

#### Strip Decomposition

Angle loss:
$$L_\phi = 1 - \cos(\phi - \phi^*)$$

Width loss:
$$L_w = \text{KL}(w^* / W_{\text{total}} \| w / W_{\text{total}})$$

Total:
$$L = \lambda_\phi L_\phi + \lambda_w L_w$$

#### Set-Based Regions (DETR)

Hungarian matching:
$$\sigma^* = \arg\min_\sigma \sum_i C(R_i, R^*_{\sigma(i)})$$

Cost:

- L1 box loss
- angle loss
- existence classification

### B2.4 Why Supervision Is Limited

- oracle bias
- expensive labels
- poor generalization under shift

Therefore: **supervision is scaffolding, not foundation**.

## B3. Reinforcement Learning (Primary Driver)

RL is where geometry becomes *task-aware*.

### B3.1 Environment Definition

State:
$$s = (P(x,y), \text{geometry so far}, \text{coverage mask})$$

Action:

- geometry parameters (all at once or sequentially)

Episode:

- single geometry output (set-based)
- or sequential region generation

### B3.2 Reward Design

For region $R_i$:
$$r_i = \int_{R_i \setminus \cup_{j<i} R_j} P(x,y)\,dx\,dy - \lambda \cdot \text{time}(R_i) - \mu \cdot \text{overlap}(R_i)$$

Terminal bonus:
$$r_T = -\kappa \cdot (1 - \text{coverage})$$

### B3.3 Algorithms

- REINFORCE (baseline, high variance)
- PPO (default)
- SAC (continuous actions, harder)

Key point:

> **Reward exists only during training.**

### B3.4 Stability Mechanisms

- curriculum: start with smooth $P$
- entropy regularization
- reward normalization
- early strip-only training

## B4. Self-Supervised / Energy-Based Training

Used when labels are unavailable.

### B4.1 Energy Definition

Define energy over geometry:
$$E(\{R_i\}) = -\int P(x,y)\,\mathbf{1}_{\text{covered}}\,dx\,dy + \lambda \cdot T + \mu \cdot \text{overlap}$$

### B4.2 Training Objective

Train generator $G_\theta$ to minimize expected energy:
$$\min_\theta \mathbb{E}_{R \sim G_\theta(P)}[E(R)]$$

This is:

- black-box
- differentiable only through generator
- evaluated offline

### B4.3 Why This Works

The generator learns:

- geometry that "looks good"
- without explicit targets
- objective is compiled into weights

## B5. Autoregressive Training (When Needed)

Used only when:

- geometry must adapt sequentially
- coverage mask is critical

State update:
$$M_{t+1} = M_t \cup R_t$$

STOP condition:

- learned
- or coverage threshold

## B6. What Training Must Never Do

1. Enforce hard constraints via penalties alone
2. Rely on inference-time optimization
3. Assume reward smoothness
4. Hide failure cases

---

# Section C — Hybridization with Classical Optimization

This section defines **explicit contracts** between learning and optimization.

## C1. Why Hybridization Is Mandatory

Neural models:

- learn global structure
- fail at exact feasibility

Classical solvers:

- enforce constraints
- scale poorly from scratch

Hybridization is **structural**, not optional.

## C2. Interface Contract

Neural output must produce:

| Property | Required |
|----------|----------|
| Finite region set | Yes |
| Searchable shapes | Yes |
| Region cost estimate | Yes |
| Region reward estimate | Yes |
| Repairable geometry | Yes |

Nothing else.

## C3. Routing Layer

Each region becomes a node:

- reward = probability mass
- cost = sweep time

Problem reduces to:

- Orienteering
- VRP
- Multi-vehicle assignment

Solved by:

- ALNS
- 2-opt / Or-opt
- deterministic heuristics

## C4. Repair Responsibilities

Neural model is **never retrained** for:

- infeasible fuel
- uncovered gaps
- late-stage failures

Repair rules are:

- explicit
- deterministic
- auditable

## C5. Performance Claim

> **Neural geometry reduces routing search space enough that classical methods succeed quickly.**

This is empirically testable.

---

# Section D — Multi-Vehicle, No-Communication Execution

## D1. Problem Setting

- $N$ vehicles
- plan jointly
- execute independently
- failures possible

## D2. Why This Is Hard

Redundancy tradeoffs are combinatorial:

- overlap vs efficiency
- risk vs coverage

Hand-coded heuristics fail.

## D3. Learning Robust Geometry

Training:

- sample vehicle failures
- reward = POD of survivors

Effect:

- network learns where redundancy matters

## D4. Execution Guarantees

- no runtime coordination
- failure tolerance learned
- still repairable post-hoc

---

# Section E — Failure Modes and When Not to Use Learning

This section is **required for honesty**.

## E1. Learning Fails When

- $|A|$ small → brute force works
- single instance → no amortization
- unimodal $P$ → spiral sweep trivial
- insufficient data
- extreme domain shift

## E2. Sanity Test

If:
$$\text{Greedy} \ge 0.9 \times \text{Best Known}$$

Do **not** use learning.

---

# Section F — Design Doctrine (Non-Negotiable Rules)

1. Geometry first
2. Guarantees before expressiveness
3. Repair beats penalty tuning
4. Learning compiles structure, not constraints
5. Never trust a model you can't fix

---

# Appendix F — Cost Models and Deterministic Repair Guarantees

This appendix makes the system **implementable** and **auditable** by (1) pinning down concrete time / fuel models and (2) stating exactly what the repair layer guarantees (and what it cannot).

## F1. Cost Models

We split "cost" into **(A) transit** and **(B) on-region coverage**, because they behave differently and correspond to different constraints (range, battery, duty cycle, etc.).

### F1.1 Notation

Vehicle $k$ has:

- speed $v_k$ (m/s)
- sweep width $W_k$ (m) (effective sensor footprint per pass)
- endurance/budget $B_k$ (seconds) or energy $E_k$
- turning penalty model $T^{\text{turn}}_k(\cdot)$ (optional)

Region $R$ has:

- area $\text{Area}(R)$
- "span length" $L_R$ along sweep direction (for lawnmower)
- "span width" $U_R$ perpendicular to sweep direction (for lawnmower)
- perimeter $\text{Per}(R)$ (for boundary overhead approximations)

We also define a **coverage intensity** $C(x)$ (dimensionless) or equivalently a desired number of passes / dwell time mapping.

## F2. Coverage Time Models

### F2.1 Baseline "Area Law" (Most Stable)

If coverage is achieved by sweeping with effective width $W_k$, the dominant term is:

$$T^{\text{cov}}_k(R) \approx \frac{\text{Area}(R)}{v_k W_k}$$

**Why this is a good default**

- It is invariant to region aspect ratio (to first order).
- It is robust across sensor platforms.
- It matches strip decomposition naturally.

**Bound (necessary condition):**
No plan can cover region $R$ faster than this up to a constant factor, because each unit of time covers at most $v_k W_k$ area.

### F2.2 Lawnmower Path Length Model (Rectangles, Strips)

For a region that is approximately a rectangle with:

- width $U_R$ perpendicular to sweep direction,
- length $L_R$ along sweep direction,

the number of passes is roughly $n = \lceil U_R / W_k \rceil$.
Total path length:

$$\ell^{\text{cov}}(R) \approx n \cdot L_R + \ell^{\text{turn}}(n)$$

Then:

$$T^{\text{cov}}_k(R) \approx \frac{\ell^{\text{cov}}(R)}{v_k}$$

Turning overhead models:

- **Constant-per-turn**: $\ell^{\text{turn}}(n) = (n-1) d_k$, with $d_k$ a vehicle-specific turn penalty distance.
- **Curvature-limited**: if min turn radius is $r_k$, a hairpin turn costs $\approx \pi r_k$, so $\ell^{\text{turn}}(n) \approx (n-1)\pi r_k$.

### F2.3 Terrain / Sensor Weighting (Nonuniform Coverage Demand)

Sometimes "coverage" is not uniform; you want more effort in high-probability areas. A clean model is to define a desired **coverage intensity field** $C^*(x)$ proportional to $P(x)$ (or a saturating transform of it), and define time as:

$$T^{\text{cov}}_k(R) \approx \frac{1}{v_k W_k}\int_R \rho(x)\,dx$$

where $\rho(x)$ is "required sweep effort density," e.g.

- $\rho(x)=1$ (uniform)
- $\rho(x)=1+\beta \cdot \tilde{P}(x)$ (more effort in high-$P$)
- $\rho(x)=\min(\rho_{\max}, 1+\beta \tilde{P}(x))$ (cap effort)

This keeps a linear-time cost that's differentiable and compositional.

## F3. Transit Time and Full Route Budget

Let route for vehicle $k$ be an ordered list of regions $(R_{k,1},\dots,R_{k,m})$. Let $c(R)$ be a chosen "service point" for region $R$ (centroid, entry point, or nearest boundary point). Then transit length:

$$\ell^{\text{trans}}_k \approx d(\text{base}_k, c(R_{k,1})) + \sum_{j=1}^{m-1} d(c(R_{k,j}), c(R_{k,j+1})) + d(c(R_{k,m}), \text{base}_k)$$

Transit time:
$$T^{\text{trans}}_k = \frac{\ell^{\text{trans}}_k}{v_k}$$

Total mission time:
$$T^{\text{tot}}_k = T^{\text{trans}}_k + \sum_{j=1}^m T^{\text{cov}}_k(R_{k,j})$$

Hard constraint:
$$T^{\text{tot}}_k \le B_k$$

## F4. POD / Reward Computation (Training Only)

A practical POD proxy compatible with the diminishing-returns model is:

### F4.1 Coverage Intensity Aggregation

Let each region search contributes an intensity $C_R(x)$ (often constant within $R$). Cumulative intensity:
$$C(x)=\sum_{R \in \text{searched}} C_R(x)$$

Then training reward:
$$\text{POD} = \int_A P(x)\left(1-e^{-C(x)}\right)dx$$

For simplest strip model, let each strip yields constant intensity proportional to time spent:
$$C_R(x) = \gamma \cdot \frac{T_R v_k W_k}{\text{Area}(R)} \mathbf{1}_R(x)$$

so that more time implies larger intensity, but with controlled scaling.

## F5. Deterministic Repair Layer

Repair exists to guarantee hard constraints and make the pipeline *deployable*. It must be:

- deterministic
- bounded-time
- auditable
- geometry-only (no learning)

We separate repair into two types:

1. **Coverage repair** (gaps)
2. **Feasibility repair** (budget violations)

## F6. Coverage Repair Guarantees

### F6.1 Strip Representation — Repair Usually Unnecessary

From Section A, strip decomposition covers $A$ by construction. Only practical failures come from:

- numeric precision
- polygon clipping implementation bugs
- rasterization approximations (training-time metric, not true coverage)

**Deterministic guard:** after geometry build, compute an exact coverage check in continuous geometry:

- For strips: coverage is guaranteed if `WidthTotal` and boundaries computed correctly.
- For polygons: clip each strip with $A$ exactly; union of clipped strips equals $A$.

### F6.2 General Shapes (Rectangles/Polygons) — Repair via Gap Components

Let predicted regions be $\{R_i\}$. Define uncovered set:
$$G = A \setminus \bigcup_i R_i$$

Repair options:

#### Repair Option 1: Add Bounding Shapes for Each Gap Component

Compute connected components $G_1,\dots,G_q$. For each, add a rectangle (axis-aligned or rotated) that covers it:
$$\widehat{R}_j \supseteq G_j$$

Then new set:
$$\{R_i\}' = \{R_i\} \cup \{\widehat{R}_j\}_{j=1}^q$$

**Guarantee:** $\cup \{R_i\}' \supseteq A$ (coverage satisfied).

#### Repair Option 2: Expand Nearest Region

For each gap component $G_j$, find region $R_{i(j)}$ minimizing distance to $G_j$, and expand it minimally to cover $G_j$. This can be done by expanding the rectangle's width/height or polygon support function.

**Guarantee:** coverage satisfied if expansion is computed to include $G_j$.

### F6.3 Proof of Coverage Guarantee for Repair Option 1

Let $U = \cup_i R_i$. By definition, $G = A \setminus U$. Let $\{\widehat{R}_j\}$ be such that $G \subseteq \cup_j \widehat{R}_j$. Then:
$$A = (A \cap U) \cup (A \cap G) \subseteq U \cup \left(\cup_j \widehat{R}_j\right)$$

So repaired union covers $A$. ∎

## F7. Feasibility Repair Guarantees (Budgets)

Feasibility means meeting per-vehicle budget constraints $T^{\text{tot}}_k \le B_k$. Repair can happen at two levels:

1. **Routing-level repair** (preferred): adjust assignment/order of regions via ALNS
2. **Geometry-level repair** (fallback): reduce region set or simplify

### F7.1 Routing-Level Repair (ALNS as Feasibility Engine)

Given regions with costs $c(R)$ and rewards $r(R)$, ALNS maintains feasible solutions by:

- removing expensive regions
- re-inserting under constraints
- swapping between vehicles

**Guarantee (conditional):** If there exists any feasible assignment of the given regions, a complete search method could find it; ALNS is heuristic, so no absolute guarantee. But you can enforce *always-feasible-by-construction* by:

- Start with empty routes (feasible)
- Only insert a region if budget remains (feasibility-preserving insertion)
- Allow removal moves that always restore feasibility

This yields the invariant:

> **Invariant:** routes remain feasible at every iteration.

### F7.2 Geometry-Level Repair: Drop / Merge / Coarsen

If the region set itself is infeasible to execute (too many regions, too large total coverage time), you must alter geometry.

Deterministic strategies:

#### Strategy A: Drop Lowest-Value Regions (if full coverage not required)

Sort regions by value density:
$$\eta(R) = \frac{r(R)}{c(R)}$$

Remove lowest $\eta$ until feasible.

**Guarantee:** feasibility achieved if removal can reduce cost enough. Coverage may be violated; only valid for "maximize POD under budget," not mandatory coverage.

#### Strategy B: Merge Adjacent Regions

Merge $R_i, R_j$ into $R_{ij}$ (union or bounding shape). This reduces routing overhead and region count.

#### Strategy C: Fallback to Strip Decomposition (Hard Safety Fallback)

If the learned set is infeasible or messy, deterministically replace with strips:

- choose $\phi$ from model (or from a simple heuristic)
- choose uniform widths
- guarantee coverage
- then route strips

This is the strongest practical safety measure:

> **Always have a feasible, coverage-guaranteed baseline.**

## F8. What Repair Cannot Guarantee

Repair can guarantee **coverage** (by adding/expanding regions) and can guarantee **budget feasibility** if you allow dropping regions (when full coverage isn't required). But repair cannot guarantee all of these simultaneously:

- full coverage
- strict budget feasibility
- minimal overlap
- near-optimal POD

If full coverage is mandatory and budgets are too small, no algorithm can satisfy the constraints: the problem is infeasible.

**Feasibility certificate (necessary condition):**
If even the theoretical minimum coverage time exceeds budget, infeasible:

For vehicle $k$ alone covering all of $A$, a necessary condition is:
$$\frac{\text{Area}(A)}{v_k W_k} \le B_k$$

For multiple vehicles, necessary condition:
$$\sum_k v_k W_k B_k \ge \text{Area}(A)$$

(up to turn inefficiency constants). If this fails, full coverage is impossible.

## F9. Pseudo-code: Deterministic Repair (Reference Implementation Level)

### F9.1 Coverage Repair for Set-Based Regions

```
function CoverageRepair(A, regions):
    U = Union(regions)
    G = Difference(A, U)
    if IsEmpty(G):
        return regions

    comps = ConnectedComponents(G)
    patches = []
    for comp in comps:
        patch = BoundingRectangle(comp)
        patches.append(patch)

    return regions ∪ patches
```

### F9.2 Feasibility Repair via Feasibility-Preserving Insertion

```
function RouteRepair_FeasibleInsert(vehicles, regions):
    routes = {k: empty}
    remaining = {k: Bk}

    regions_sorted = sort_desc(regions, key = r(R)/c(R))

    for R in regions_sorted:
        best_k = None
        best_cost = +inf
        for k in vehicles:
            cost = TransitIncrement(routes[k], R, k) + CoverageCost(R,k)
            if cost <= remaining[k] and cost < best_cost:
                best_cost = cost
                best_k = k

        if best_k != None:
            Insert(routes[best_k], R)
            remaining[best_k] -= best_cost

    return routes
```

### F9.3 Hard Fallback to Strips

```
function FallbackToStrips(P, A, vehicle_params):
    φ = HeuristicAngle(P, A) or ModelAngle(P)
    w = UniformWidths(N, WidthTotal(A, φ))
    strips = BuildStrips(A, φ, w)
    return strips
```

## F10. Practical Recommendations

1. Use **area law** as default cost until you have real sensor models.
2. Enforce **always-feasible routing** by feasibility-preserving insertion + ALNS improvements.
3. Treat strip fallback as a **safety mechanism**, not an embarrassment.
4. Validate feasibility with the **capacity bound** before doing anything expensive.

---

# Appendix G — Exact Evaluation vs. Approximations

This appendix answers a single practical question:

> *How do we evaluate coverage, POD, and rewards accurately enough to train — without making training unstable, slow, or misleading?*

## G1. Quantities That Must Be Evaluated

Across training and diagnostics, the system repeatedly needs estimates of:

1. **Coverage fraction**
   $$\text{cov} = \frac{\text{Area}\left(\bigcup_i R_i \cap A\right)}{\text{Area}(A)}$$

2. **Probability mass covered**
   $$M = \int_A P(x)\,\mathbf{1}_{\cup_i R_i}(x)\,dx$$

3. **POD proxy**
   $$\text{POD} = \int_A P(x)\left(1-e^{-C(x)}\right)\,dx$$

4. **Incremental gain**
   $$\Delta M(R \mid \mathcal{S}) = \int_{R \setminus \cup_{S\in\mathcal{S}} S} P(x)\,dx$$

These are **geometric integrals**. Exact evaluation is possible but often unnecessary.

## G2. Exact Evaluation (Ground Truth, Debug, Oracle)

### G2.1 Exact Area and Coverage

If all regions $R_i$ and $A$ are polygons:

1. Compute exact polygon unions: $U = \bigcup_i R_i$
2. Compute exact intersection: $U_A = U \cap A$
3. Compute exact area via polygon area formula.

**Complexity**

- Union of $n$ polygons: worst-case $O(n^2)$ vertices
- Practical cost grows rapidly with fragmentation

**When to use**

- validation
- unit tests
- oracle data generation
- verifying repair correctness

**When *not* to use**

- inner training loop
- RL reward computation

## G3. Grid-Based (Raster) Approximation

This is the **workhorse** for most training.

### G3.1 Uniform Grid Approximation

Let grid cell size be $\delta \times \delta$. Let grid points $u_j$ cover bounding box of $A$.

Approximate:
$$\int_A f(x)\,dx \approx \delta^2 \sum_j f(u_j)\,\mathbf{1}_A(u_j)$$

Used for:

- coverage
- probability mass
- POD proxy

### G3.2 Error Bound (Area & Coverage)

Let $f=\mathbf{1}_{\cup_i R_i \cap A}$. Then:

$$\left| \int f(x)\,dx - \delta^2 \sum_j f(u_j) \right| \le c\,\delta\,\text{Per}(\partial(\cup_i R_i \cap A))$$

where:

- $\text{Per}(\cdot)$ = perimeter
- $c$ depends on rasterization convention (≤2 in practice)

**Implications**

- Error scales **linearly** with resolution
- Error scales with **boundary complexity**

This is why:

- strip decompositions are stable (small perimeter)
- arbitrary polygons are noisy (large perimeter)

### G3.3 Practical Rule of Thumb

Let acceptable absolute error be $\varepsilon$. Choose:
$$\delta \lesssim \frac{\varepsilon}{\max(P_{\max}\,\text{Per}, L_P\,\text{Area})}$$

In practice:

- start coarse (e.g., $128 \times 128$)
- anneal resolution only if gradients are noisy
- **never** chase numerical exactness during RL

## G4. Monte Carlo Estimation

Monte Carlo (MC) replaces grid sums with random samples.

### G4.1 Basic Monte Carlo Estimator

Sample $x_1,\dots,x_N \sim \text{Uniform}(A)$.

Estimate:
$$\widehat{M} = \frac{\text{Area}(A)}{N} \sum_{i=1}^N P(x_i)\,\mathbf{1}_{\cup_i R_i}(x_i)$$

Unbiased estimator.

### G4.2 Variance Bound

Let:
$$Y = P(X)\mathbf{1}_{\cup_i R_i}(X) \quad X \sim \text{Uniform}(A)$$

Then:
$$\text{Var}(\widehat{M}) = \frac{\text{Area}(A)^2}{N} \text{Var}(Y) \le \frac{\text{Area}(A)^2}{N} P_{\max}^2$$

So RMS error:
$$\text{RMSE} \le \frac{\text{Area}(A)P_{\max}}{\sqrt{N}}$$

### G4.3 MC vs Grid: Trade-off

| Aspect | Grid | Monte Carlo |
|--------|------|-------------|
| Bias | Yes (controlled) | No |
| Variance | Low | High (unless large N) |
| Gradients | Smooth | Noisy |
| SIMD/GPU | Excellent | Good |
| Geometry complexity | Sensitive | Insensitive |

**Rule**

- Use **grid** for gradients
- Use **MC** for diagnostics and validation

## G5. Differentiability Considerations

### G5.1 Non-Differentiability of Hard Masks

Hard indicator:
$$\mathbf{1}_{R}(x)$$

has zero gradient almost everywhere.

**Never use directly in gradient-based training.**

### G5.2 Soft Rasterization

Define signed distance $d_R(x)$ (negative inside).

Soft mask:
$$m_R(x) = \sigma\left(-\frac{d_R(x)}{\tau}\right)$$

where:

- $\sigma$ = logistic sigmoid
- $\tau$ = temperature

Properties:

- differentiable everywhere
- converges to hard mask as $\tau \to 0$

### G5.3 Union Approximation

Two common forms:

**Max**
$$m(x) = \max_i m_{R_i}(x)$$

- sharp
- non-smooth

**Noisy-OR**
$$m(x) = 1 - \prod_i (1 - m_{R_i}(x))$$

- smooth
- bounded
- overestimates overlap softly

**Recommendation**

- noisy-OR for training
- hard max or exact union for evaluation

## G6. POD Approximation Stability

Recall:
$$\text{POD} = \int P(x)(1-e^{-C(x)})dx$$

### G6.1 Linearized Proxy

For small $C$:
$$1-e^{-C} \approx C$$

So proxy:
$$\text{POD}_{\text{lin}} = \int P(x)C(x)\,dx$$

**Bound**
$$0 \le (1-e^{-C}) - C \le \frac{C^2}{2}$$

Thus:

- linear proxy **overestimates** marginal gains at high coverage
- but is stable and smooth

### G6.2 Squared-Mass Heuristic

Used in strip self-supervision:

$$J = \sum_i \frac{\left(\int_{R_i} P(x)\,dx\right)^2}{\text{Area}(R_i)}$$

This rewards:

- narrow strips in high-density areas
- spreading mass across regions

## G7. Training-Time vs Evaluation-Time Fidelity

### G7.1 Acceptable Inaccuracies During Training

Training **does not require**:

- exact coverage fraction
- exact POD
- exact travel time

It requires:

- *correct gradients*
- *consistent ordering*
- *low-variance signals*

### G7.2 Evaluation Must Be Stricter

For reporting / validation:

- use finer grids or MC
- compare against exact on small cases
- never trust training metrics alone

## G8. Reference Pseudo-Code: Evaluation Kernels

### G8.1 Grid-Based Coverage + Mass

```
function GridEval(P_grid, mask_grid, A_mask, δ):
    covered = mask_grid * A_mask
    cov_frac = sum(covered) / sum(A_mask)
    mass = δ^2 * sum(P_grid * covered)
    return cov_frac, mass
```

### G8.2 Monte Carlo Mass

```
function MonteCarloMass(P, regions, A_sampler, N):
    sum = 0
    for i in 1..N:
        x = SampleUniform(A_sampler)
        if InAnyRegion(x, regions):
            sum += P(x)
    return Area(A_sampler) * sum / N
```

### G8.3 POD Proxy (Grid)

```
function POD_Grid(P_grid, C_grid, A_mask, δ):
    pod = δ^2 * sum( P_grid * (1 - exp(-C_grid)) * A_mask )
    return pod
```

## G9. Final Doctrine for Evaluation

1. **Exact geometry is for truth, not training**
2. **Grid approximations are default**
3. **Monte Carlo is for validation**
4. **Never optimize what you can't approximate stably**
5. **Geometry choice controls evaluation stability**

---

# Appendix H — Complexity & Scaling Laws (End-to-End)

This appendix answers:

> *Where does the computation actually go, how does it scale, and what breaks first as problem size grows?*

## H1. Problem Size Parameters

| Symbol | Meaning |
|--------|---------|
| $A$ | Search region area |
| $n_p$ | Grid resolution (cells per side) |
| $N = n_p^2$ | Number of grid cells |
| $K$ | Number of regions generated (strips / rectangles) |
| $V$ | Number of vehicles |
| $T$ | Training iterations |
| $B$ | Batch size |
| $M$ | Monte Carlo samples (if used) |

## H2. Geometry Generation Complexity

### H2.1 Strip Decomposition

**Forward pass**

- CNN encoder: $O(N)$ per instance
- Angle + width heads: $O(K)$

**Geometry construction**

- Projection of polygon vertices: $O(|\partial A|)$ (constant in practice)
- Strip construction: $O(K)$

**Total inference cost**
$$O(N + K)$$

**Key scaling insight**

- Dominated by **grid resolution**, not number of regions
- $K$ can be kept small (e.g., 8–32) independent of $A$

### H2.2 Set-Based Regions (DETR-style)

**Forward pass**

- CNN backbone: $O(N)$
- Transformer encoder: $O(Nd^2)$ (or $O(Nd)$ with linear attention)
- Transformer decoder: $O(K^2 d)$

**Training-only matching**

- Hungarian algorithm: $O(K^3)$

**Inference**
$$O(N + K^2)$$

**Training**
$$O(N + K^3)$$

**Scaling limit**

- Hungarian matching becomes problematic for $K \gtrsim 100$
- Geometry fragmentation (large $K$) is the *true bottleneck*, not routing

## H3. Evaluation Cost Scaling

### H3.1 Grid-Based Evaluation

Coverage, mass, POD via grids:
$$O(N)$$

Dominant constants:

- mask composition
- exponentials for POD
- memory bandwidth

**Key rule**

> Grid resolution dominates *everything*.

### H3.2 Monte Carlo Evaluation

$$O(M \cdot K)$$

- Cheap geometry tests (point-in-rect / strip)
- Scales well with complex shapes
- High variance unless $M$ large

Used only for:

- diagnostics
- validation
- oracle comparison

## H4. Training Complexity (By Paradigm)

### H4.1 Supervised (Strips)

$$O(T \cdot B \cdot (N + K))$$

Very stable, fully GPU-bound.

### H4.2 RL / PPO (Single-Shot Geometry)

Per iteration:

- rollout eval: $O(B \cdot (N + K))$
- PPO updates: $O(B \cdot d^2)$

Total:
$$O(T \cdot B \cdot (N + K))$$

**Key observation**
RL adds a **constant factor**, not new asymptotics.

### H4.3 Autoregressive RL

Sequential generation of $K$ regions:
$$O(T \cdot B \cdot K \cdot (N + 1))$$

**Scaling limit**

- Linear in $K$
- Unnecessary unless sequential dependency is essential

### H4.4 Energy-Based / Black-Box

$$O(T \cdot B \cdot (N + K))$$

But with **high variance constants** due to sampling.

### H4.5 Diffusion Models (If Used)

Training:
$$O(T \cdot B \cdot T_{\text{diff}} \cdot d)$$

Inference:
$$O(T_{\text{diff}} \cdot d)$$

Where $T_{\text{diff}} \sim 20$–1000.

**Conclusion**
Diffusion is orders of magnitude heavier than strip or DETR models and should only be pursued if expressiveness demands it.

## H5. Routing & Hybrid Layer Complexity

### H5.1 Region Graph Construction

$$O(K^2)$$

(distance matrix)

Negligible for $K \le 100$.

### H5.2 ALNS Routing

Let $I$ be number of iterations.
$$O(I \cdot K^2)$$

Typical:

- $I \sim 10^2$–$10^3$
- Fast because starting solution is good

**Critical insight**

> Geometry quality reduces the *effective* $I$, not the asymptotic.

## H6. End-to-End Inference Cost

### H6.1 Strip-Based Pipeline

$$O(N + K + V K^2)$$

For fixed $K$, linear in grid resolution.

### H6.2 Set-Based Pipeline

$$O(N + K^2 + V K^2)$$

Still dominated by $N$ unless geometry fragments badly.

## H7. Scaling Laws & Failure Thresholds

### H7.1 Resolution Scaling

Doubling linear grid resolution ($n_p \to 2n_p$):

- $N \to 4N$
- runtime $\approx 4\times$

**Implication**

- Always start coarse
- Increase resolution only for evaluation

### H7.2 Geometry Fragmentation Threshold

Empirically:

- $K \lesssim 32$: everything stable
- $32 < K < 100$: DETR matching + routing slow
- $K \gg 100$: system degrades catastrophically

**Design doctrine**

> Fix bad geometry before optimizing solvers.

### H7.3 Vehicle Scaling

Vehicles scale *linearly*:
$$O(V \cdot K^2)$$

Multi-vehicle coordination cost is negligible relative to geometry generation.

## H8. Summary of What Actually Limits Scale

| Component | Bottleneck |
|-----------|------------|
| Training | Grid resolution |
| Inference | Geometry fragmentation |
| Routing | Poor geometry |
| Evaluation | Over-resolution |
| Robustness | Distribution shift |

---

# Appendix I — Ablation Logic & Failure Diagnostics

This appendix defines **what to ablate, why, and what conclusions are valid**.

The goal is not leaderboard optimization, but **structural falsification**.

## I1. Mandatory Baselines (Non-Negotiable)

Every experiment must include:

1. **Uniform strips**
2. **Greedy by $P(x)/\text{distance}$**
3. **Random geometry + ALNS**
4. **Exact / oracle (small cases)**

If learned methods cannot beat (1) or (2), stop.

## I2. Geometry Ablations (Primary)

### I2.1 Strip Count ($K$)

Vary $K \in \{4, 8, 16, 32\}$.

**Expected behavior**

- Small $K$: underfit geometry
- Large $K$: overfit noise, routing overhead

**Failure signal**

- POD improves but routing cost explodes → geometry fragmentation

### I2.2 Angle Learning vs Fixed Angle

Compare:

- learned $\phi$
- fixed $\phi=0$
- PCA-based $\phi$

**Conclusion logic**

- If learned $\phi$ does not beat PCA, geometry learning is weak
- If PCA beats learning, training signal is wrong

### I2.3 Width Learning vs Uniform

Compare:

- learned $w_i$
- uniform $w_i = W_{\text{total}}/K$

**Expected**

- Gains only on multimodal $P$
- No gain on unimodal $P$

## I3. Training Paradigm Ablations

### I3.1 Supervised → RL Transition

Measure:

- convergence speed
- final POD
- variance across seeds

**Diagnostic**

- RL without supervision diverges → reward too noisy
- Supervised only saturates early → labels insufficient

### I3.2 Reward Term Removal

Remove one term at a time:

- coverage penalty
- overlap penalty
- time penalty

**Interpretation**

- Overlap explodes → missing overlap term
- Strips collapse → missing regularization
- Geometry trivializes → reward mis-scaled

## I4. Evaluation Ablations

### I4.1 Grid Resolution Sensitivity

Train at fixed resolution, evaluate at finer grids.

**Failure signal**

- Large performance drop → learned geometry exploiting grid artifacts

### I4.2 MC vs Grid Consistency

Compare MC-estimated mass vs grid.

**Failure signal**

- Rank order changes → evaluation unstable

## I5. Distribution Shift Diagnostics

### I5.1 Shift Types

Test on:

- sharper $P$
- flatter $P$
- rotated terrain
- multimodal with new orientations

**Expected**

- graceful degradation, not collapse

### I5.2 Failure Mode Classification

| Symptom | Root Cause |
|---------|------------|
| Always picks same angle | Mode collapse |
| Over-fragmented regions | Overfitting noise |
| Excess overlap | Weak penalties |
| Misses low-P areas | Greedy bias |
| Infeasible routes | Geometry too fine |

## I6. Multi-Vehicle Ablations

### I6.1 Failure Probability Sweep

Train with $p_{\text{fail}} \in \{0, 0.1, 0.3, 0.5\}$.

Evaluate robustness.

**Expected**

- Redundancy increases only in high-value regions

### I6.2 Communication Removal Test

Compare:

- joint planning
- independent planning

If independent planning is competitive, coordination learning failed.

## I7. Sanity Checks (Hard Stops)

Stop the project if:

1. Uniform strips ≥ learned geometry
2. Greedy ≥ learned + ALNS
3. Learned geometry worse at higher resolution
4. Performance depends critically on grid artifacts
5. Fallback strips outperform learned repair

These indicate **no real structure is being learned**.

## I8. What Positive Results Actually Mean

Only claim success if:

- Learned geometry generalizes across distributions
- Improvements persist after routing
- Gains survive evaluation tightening
- Failures are predictable and bounded

## I9. Minimal Reporting Standard

Every result must report:

- $K$
- grid resolution
- evaluation method
- fallback comparison
- feasibility rate
- failure cases

Anything less is misleading.

---

# Appendix J — Paper Extraction Map (2–4 Publications Without Overlap)

This appendix answers:

> *How do we slice the blueprint into publishable units that each make a clean claim, reuse infrastructure, and don't cannibalize one another?*

The strategy is to publish along **orthogonal axes**:

1. representation with guarantees
2. systems hybridization
3. robustness / multi-agent
4. evaluation methodology

## J0. Shared Infrastructure Across All Papers

Build once, reuse everywhere:

- Instance generator / dataset loader for $P(x,y), A$
- Geometry generators: strips + set-based (DETR)
- Deterministic repair library (coverage + feasibility fallbacks)
- Unified evaluation suite (grid + MC + exact small cases)
- Classical routing backend (ALNS + feasibility-preserving insertion)
- Reporting harness that produces:
  - POD vs time curves
  - feasibility rate
  - repair rate
  - fragmentation metrics

This is your "platform," and each paper is a different experiment on it.

---

## Paper 1 — Coverage-Guaranteed Neural Geometry via Strip Decomposition

### Thesis (Single Sentence)

A neural network can learn **coverage-guaranteed geometric decompositions** (angle + strip widths) that improve POD under hard time constraints with **objective-free inference**.

### Core novelty

- strip representation as *learnable geometry* with **formal coverage guarantee**
- training paradigms that compile the objective offline
- clear proof that geometry-first reduces downstream complexity

### What's in-scope

- strip decomposition only
- supervised + RL + self-supervised (one or two, pick the best story)
- deterministic fallback/repair (mostly no-op)

### What's out-of-scope (explicitly)

- arbitrary polygon generation
- multi-vehicle coordination
- diffusion models
- claims about optimality

### Key experiments

- compare learned $\phi$ vs uniform vs PCA-angle
- compare learned widths vs uniform widths
- show generalization across shifted $P$ distributions
- demonstrate stable evaluation at higher resolution

### Best venue fit

- robotics coverage planning: ICRA / IROS
- ML4CO workshop tracks
- applied optimization / autonomy

### "Kill criteria" (what reviewers will ask)

- does it beat uniform strips?
- does it generalize?
- are improvements stable under evaluation tightening?

---

## Paper 2 — Learn the Space, Solve the Route: Hybrid Geometry + ALNS for SAR Planning

### Thesis

Learning geometry upstream produces solutions that classic metaheuristics can refine far faster and more reliably than solving from scratch on discretized nodes.

### Core novelty

- explicit **interface contract** between neural geometry and ALNS
- empirical scaling law: geometry quality reduces ALNS iterations / failure rate
- end-to-end hybrid pipeline comparison

### What's in-scope

- strips (and optionally DETR rectangles as an ablation)
- ALNS refinement as a second stage
- hard feasibility handling and repair

### What's out-of-scope

- new ALNS operators (don't become "ALNS paper")
- multi-agent failures
- interpretability beyond geometry visualization

### Key experiments

- baseline: discretized-grid VRP + ALNS
- baseline: random geometry + ALNS
- show warm-start gains:
  - iterations to reach threshold POD
  - feasibility rate
  - runtime vs quality curves ("anytime")
- show fragmentation threshold ($K$) at which hybrid breaks

### Best venue fit

- OR / optimization venues: INFORMS Computing Society, CPAIOR
- ML4CO (NeurIPS workshop / ICML workshop)
- autonomy systems venues

### Reviewer-proofing

- demonstrate that the gain isn't "because your ALNS is bad"
- use strong ALNS baseline with tuned parameters
- report convergence curves, not just endpoint metrics

---

## Paper 3 — Robust Multi-Vehicle Planning Without Communication via Failure-Conditioned Training

### Thesis

Failure-conditioned training yields implicit redundancy strategies that outperform hand-coded redundancy heuristics in multi-vehicle, no-communication execution.

### Core novelty

- objective: maximize POD under vehicle failures
- training distribution includes failure scenarios
- learned redundancy is spatially selective (only where it matters)

### What's in-scope

- multi-vehicle strip geometry + assignment
- centralized training, decentralized execution (CTDE) framing
- evaluation under failure sweeps ($p_{\text{fail}}$)

### What's out-of-scope

- communication protocols
- real-time re-planning during flight
- heterogeneous sensors unless already stable

### Key experiments

- compare:
  - independent planning
  - joint planning without failure training
  - joint planning with failure training
  - simple redundancy heuristic (e.g., overlap top-X% regions)
- evaluate:
  - POD under 0/1/2 vehicle failures
  - redundancy distribution maps
  - efficiency loss vs robustness gain

### Best venue fit

- multi-robot systems: ICRA / IROS / RSS
- autonomy robustness workshops
- applied MARL venues (if RL is central)

### Hard requirement

You must show:

- redundancy is not uniform
- learned policies trade overlap vs efficiency intelligently
- policies don't collapse when failure distribution shifts

---

## Paper 4 — Benchmarking Coverage Planning Without Lying to Yourself: Evaluation Contracts and Failure Diagnostics

### Thesis

Most reported gains in neural coverage planning are artifacts of discretization, weak baselines, or unstable evaluation; we propose an evaluation contract and diagnostics that make results falsifiable.

### Core novelty

- evaluation doctrine:
  - grid vs MC consistency checks
  - resolution sensitivity tests
  - fragmentation metrics
  - repair rate reporting
- "contract" between training metrics and evaluation metrics
- failure taxonomy with stop conditions

### What's in-scope

- methodology paper, not algorithm paper
- your pipeline becomes a case study, not the centerpiece

### What's out-of-scope

- claiming SOTA POD
- new architectures
- deep RL contributions

### Key deliverables

- a checklist
- reference metrics
- minimal reproducible suite
- negative results showing common pitfalls

### Best venue fit

- Systems + ML evaluation tracks
- NeurIPS/ICML workshops on evaluation/reproducibility
- robotics benchmark tracks

### Why this paper matters

It makes the others harder to reject because you preempt the "artifact" critique.

---

## J1. How to Avoid Overlap Between Papers

The separation is:

- **Paper 1:** representation + guarantee + single-agent
- **Paper 2:** hybrid interface + scaling + routing
- **Paper 3:** robustness + multi-agent failure conditioning
- **Paper 4:** evaluation contract + diagnostics

To enforce separation, each paper has a *forbidden section list*:

| Paper | Forbidden rabbit holes |
|-------|------------------------|
| 1 | ALNS deep dive, multi-vehicle, diffusion |
| 2 | claiming geometry novelty beyond what Paper 1 already established |
| 3 | new routing algorithms, evaluation methodology treatise |
| 4 | new algorithm claims, "our method is best" narrative |

## J2. Minimal "Publishable-First" Execution Plan

If you want the fastest path to 2 strong papers:

1. **Paper 1** (strips + guarantee + objective-free inference)
2. **Paper 2** (hybrid warm start + ALNS scaling)

Then decide:

- if robustness is compelling → Paper 3
- if evaluation critique is a differentiator → Paper 4

## J3. What Data / Experiments You Must Have Before Submitting Anything

Across all papers, reviewers will demand:

- strong baselines (uniform strips, greedy, tuned ALNS)
- evaluation stability (resolution + MC checks)
- failure case reporting (repair rate, feasibility)
- ablations (angle vs widths vs K)

This is non-negotiable; Paper 4 formalizes it.

## J4. Optional "One Big Paper" Alternative (Not Recommended)

You *can* merge 1+2+3 into a monolith, but it becomes:

- harder to review
- harder to defend
- easier to reject ("too many moving parts")

The modular sequence is strategically superior.

---

# Implementation Roadmap

## Phase 1: Strip Baseline (2-3 weeks)

1. Implement strip decomposition with fixed uniform widths
2. Train CNN to predict optimal angle $\phi$ given $P$
3. Generate synthetic training data: random Gaussian mixture probability maps, brute-force optimal $\phi$
4. Evaluation metric: POD achieved vs oracle (optimal angle)

**Deliverable:** Working pipeline that takes $P$ → outputs $\phi$ → computes strips → achieves >90% of oracle POD

## Phase 2: Variable Strip Widths (2-3 weeks)

- Extend network to output $(\phi, w_1, \ldots, w_n)$
- Softmax normalization for widths
- Train with RL: reward = probability covered in priority-weighted order
- Compare to uniform baseline

**Deliverable:** Quantified improvement from variable widths (expect 5-15% POD gain on multimodal $P$)

## Phase 3: DETR-Style Arbitrary Shapes (4-6 weeks)

- Implement DETR architecture with N=30 region queries
- Rotated rectangle output (5 params + existence)
- Hungarian matching loss + coverage penalty
- Repair layer for coverage guarantee
- Compare to strip baseline on same test set

**Deliverable:** Determine if flexibility of arbitrary shapes justifies complexity. Document when each approach wins.

## Phase 4: Advanced Extensions

- Multi-vehicle assignment: add head for shape → vehicle ID
- Failure robustness: overlap important regions across vehicles
- Diffusion alternative: implement DIFUSCO-style model for comparison
- Dynamic replanning: fine-tune on updated $P$ as search progresses

---

# Verified Resources

## Foundational Papers

- Kool et al. (2019) "Attention, Learn to Solve Routing Problems!" — [arxiv.org/abs/1803.08475](https://arxiv.org/abs/1803.08475)

- Ropke & Pisinger (2006) "An Adaptive Large Neighborhood Search Heuristic for the Pickup and Delivery Problem with Time Windows"

- Washburn (2014) "Search and Detection" 5th ed. — definitive search theory textbook

- Koopman (1946/1980) "Search and Screening" — original WWII foundations

- Chen & Tian (2019) "Learning to Perform Local Rewriting" — [arxiv.org/abs/1810.00337](https://arxiv.org/abs/1810.00337)

- DETR: End-to-End Object Detection with Transformers (Carion et al., ECCV 2020) — [arxiv.org/abs/2005.12872](https://arxiv.org/abs/2005.12872)

- PolyGen: Autoregressive Generative Model of 3D Meshes (Nash et al., ICML 2020) — [arxiv.org/abs/2002.10880](https://arxiv.org/abs/2002.10880)

- DIFUSCO: Graph-based Diffusion Solvers for Combinatorial Optimization (Sun et al., NeurIPS 2023) — [arxiv.org/abs/2302.08224](https://arxiv.org/abs/2302.08224)

- Unsupervised Diffusion for Combinatorial Optimization (Sanokowski et al., ICML 2024) — [arxiv.org/abs/2406.01661](https://arxiv.org/abs/2406.01661)

- Coverage Path Planning Survey (Fevgas et al., Sensors 2022) — [PMC8839296](https://pmc.ncbi.nlm.nih.gov/articles/PMC8839296/)

- Neural Network Complete Coverage Path Planning (Yang & Luo, IEEE TSMCB 2004) — [IEEE 1262545](https://ieeexplore.ieee.org/document/1262545)

## Code Repositories

- Attention-Learn-to-Route: [github.com/wouterkool/attention-learn-to-route](https://github.com/wouterkool/attention-learn-to-route)

- ALNS Python library: [github.com/N-Wouda/ALNS](https://github.com/N-Wouda/ALNS)

- Awesome ML4CO (comprehensive list): [github.com/Thinklab-SJTU/awesome-ml4co](https://github.com/Thinklab-SJTU/awesome-ml4co)

- SARBayes (search theory tools): [sarbayes.org](https://sarbayes.org/)

- DETR (Facebook): [github.com/facebookresearch/detr](https://github.com/facebookresearch/detr)

- DIFUSCO: [github.com/Edward-Sun/DIFUSCO](https://github.com/Edward-Sun/DIFUSCO)

- Shapely (polygon operations): [shapely.readthedocs.io](https://shapely.readthedocs.io/)

## SAR-Specific Resources

- ISRID (International Search and Rescue Incident Database) — historical SAR mission data for training

- SAROPS (Search and Rescue Optimal Planning System) — US Coast Guard operational tool, uses Fokker-Planck drift models

- Glasgow DRL-SAR (2024) — recent deep RL for SAR: [frontiersin.org/articles/10.3389/frobt.2024.1527095](https://www.frontiersin.org/articles/10.3389/frobt.2024.1527095)

## Computational Geometry Libraries

- **Shapely (Python):** Polygon creation, intersection, union, area. CPU-based, very mature.

- **PyTorch Geometric:** Graph neural networks. Useful if encoding $P$ as graph.

- **Kornia:** Differentiable computer vision ops in PyTorch. Includes rotation, affine transforms.

- **SciPy:** scipy.optimize.linear_sum_assignment for Hungarian matching.

- **CGAL (C++):** Industrial-strength computational geometry. Polygon decomposition, convex hull, etc.
