# Errata Review — Asianometry, "How Nvidia Won AI"

**Source under review:** YouTube video `GuV-HyslPxk`, published February 2022, ~17:45 runtime.
**Review input:** User-supplied transcript (auto-captions plus interstitial YouTube chapter titles), provided 2026-07-09.
**Review date:** 2026-07-09.
**Reviewer:** Claude (Fable 5), with one finding (F8) caught by Souldog and sustained on challenge.
**Method:** Claims checked against reviewer knowledge (reliable through Jan 2026) and against primary/secondary sources retrieved earlier in session (CHM AlexNet source-code release materials, IEEE Spectrum, Wikipedia CUDA/AlexNet, Nvidia press/blog material, Tom's Hardware Ian Buck interview). Claims that could not be independently anchored are marked as such rather than silently passed. Transcript wording is paraphrased throughout (timestamps locate exact wording); one short verbatim quote appears in F8 where precision is load-bearing.

**Severity scale:** Moderate = materially wrong and likely to propagate if inherited. Low = wrong or misleading in detail; limited blast radius. Info = imprecision, framing hazard, or nuance worth recording.

---

## Summary register

| ID | Loc (m:ss) | Topic | Severity | Status |
|----|-----------|-------|----------|--------|
| F1 | 7:55 | OpenCL called "open source"; anachronistic placement | Moderate | Confirmed |
| F2 | 9:50 | ImageNet size, stewardship, origin | Low | Confirmed |
| F3 | 15:02 | $33/hr cloud figure, wrong unit | Low | Confirmed |
| F4 | 13:08 | CPU-vs-GPU training arithmetic | Low | Confirmed |
| F5 | 7:21 | CUDA "2006" dating | Info | Confirmed |
| F6 | 8:49–9:41 | Dennard scaling / FinFET framing | Info | Confirmed |
| F7 | ~0:40 | Chapter title year "1996" | Info | Confirmed |
| F8 | 5:02 | Parallelism narrated as an Nvidia invention | Moderate | Confirmed (user-caught) |
| F9 | 7:42 | Fixed-function pipeline "completely wiped away" | Low | Confirmed |
| F10 | 10:16 | "Top-5" metric garbled | Info | Confirmed |
| F11 | 1:04 | "World's first GPU" adopted as fact | Info | Confirmed |
| F12 | 4:04 | 1996 Nvidia product framing; NV1 erased | Info | Confirmed |
| F13 | 6:54 | GeForce 3 shaders "replaced" T&L | Info | Confirmed |

Reviewer errata: R1 (false recall of video contents), R2 (provenance mislabel on F7 artifact). Both below.

---

## Detailed findings

### F1 — Moderate — OpenCL is not open source, and did not exist in 2006 (7:55)

**Claim (paraphrased):** In the paragraph introducing CUDA (2006), OpenCL is described as CUDA's open-source cousin, jointly credited with turning commodity GPUs into general processors.

**What's wrong, part (a) — category error.** OpenCL is an **open standard**: a royalty-free specification governed by the Khronos Group. That is not the same thing as open source. The dominant OpenCL implementations of the era — Nvidia's, AMD's, Intel's, Apple's — were proprietary drivers implementing a public spec. Open-source implementations exist (Mesa's Clover, POCL), but they are not what "OpenCL" denotes. The distinction matters to exactly the audience this video serves: an open spec constrains the *interface*; it tells you nothing about whether you can read or modify the *implementation*.

**What's wrong, part (b) — anachronism.** The sentence sits inside the 2006 CUDA introduction, but OpenCL 1.0 was ratified by Khronos in **December 2008**, initiated by Apple, roughly two years after CUDA's announcement. The chronology is not incidental: OpenCL's programming model (kernels, work-items, work-groups, memory hierarchy) was substantially shaped by CUDA's, which had two years of field existence first. "Cousin" flattens a parent-child-adjacent relationship and erases the two-year gap during which CUDA had the general-purpose-GPU field essentially to itself.

**Correction:** CUDA announced November 2006; OpenCL 1.0 ratified December 2008 as an open (not open-source) standard, its model heavily informed by CUDA's.

---

### F2 — Low — ImageNet: size understated, stewardship oversimplified, origin erased (9:50)

**Claim (paraphrased):** ImageNet is a Stanford-maintained dataset of one million images; starting 2010 it hosted an annual classifier contest.

**What's wrong.** Three compounding imprecisions:

1. **Size.** The ~1.2–1.28M-image figure describes the **ILSVRC training subset** (1,000 classes). Full ImageNet was ~3.2M images at its CVPR 2009 debut and grew past 14M images across 20,000+ synsets. The video conflates the competition subset with the dataset.
2. **Origin.** ImageNet began under **Fei-Fei Li at Princeton** (project started 2007; published as Deng et al., CVPR 2009), built atop Princeton's WordNet lexical hierarchy — the "Net" in the name. Li moved to Stanford in 2009 and the project moved with her. "Maintained by Stanford" is true of the later period but erases the Princeton/WordNet origin entirely.
3. The contest start date (2010) is correct.

**Correction:** ImageNet: Fei-Fei Li et al., begun at Princeton on the WordNet backbone, published 2009, later stewarded from Stanford; 14M+ images total; ILSVRC used a ~1.28M-image, 1,000-class training subset; annual challenge from 2010.

**Note for Lecture 0:** This erasure is an instance of systemic pattern S2 (lineage erasure) — same mechanism as the SGI omission in F8, smaller stakes.

---

### F3 — Low — The $33/hour figure is per 8-GPU instance, not per GPU (15:02)

**Claim (paraphrased):** Data centers buy A100s at roughly $20k each and rent access at up to $33/hour.

**What's wrong.** The $33 figure matches the AWS `p4d.24xlarge` on-demand rate (~$32.77/hr in that era) — an instance bundling **eight** A100 40GB GPUs plus host CPUs, RAM, and 400 Gb/s networking. Per-GPU, that is ~$4.10/hr. Because the sentence sits directly after a per-unit $20k hardware price, the parallel construction invites the reader to bind $33/hr to a single A100 — an 8× unit error in implied rental economics.

**Correction:** ~$33/hr bought an 8×A100 instance; ~$4/GPU-hour. The $20k per-card street price is fine as an order-of-magnitude figure.

---

### F4 — Low — The CPU-vs-GPU training arithmetic is unsourced, regime-mixed, and conflates one epoch with a trained model (13:08)

**Claim (paraphrased):** A 2012-era dual ten-core CPU takes ~124 s on 128 images; extrapolated, ~11 days per million images; a GPU is 8.5× faster (a million images in about a day), and up to 40× in other cases.

**Assessment.**

- **Internal arithmetic:** consistent. 124 s / 128 images ≈ 0.97 s/image → ~11.2 days per 10⁶ images; ÷8.5 ≈ 1.3 days. The narration's own numbers cohere.
- **Provenance:** none given. No network, framework, batch size, or precision is specified, and training throughput varies by all four. The figures cannot be checked because they do not identify what they measure.
- **Epoch/model conflation:** processing a million images once is **one epoch**, not a trained model. AlexNet trained for roughly 90 epochs (per the original paper) in its 5–6 days on two GTX 580s. Taken at face value, the video's "GPU does a million images per day" figure would put an AlexNet-class training run at ~3 months — flatly inconsistent with the AlexNet datapoint the video itself states 90 seconds earlier. The two passages can only coexist if they describe very different networks, which is exactly the information withheld.
- **The 8.5×/40× spread:** within the range of contemporaneous literature (roughly 5×–50× depending on kernel and on how honestly the CPU baseline was optimized — cf. Intel's 2010 "Debunking the 100X GPU vs. CPU myth" pushback), so not false, just unanchored.

**Disposition:** Treat the entire numeric block as illustrative rhetoric. Do not carry any of the four numbers into derivative material. The AlexNet paper's own figures (1.2M images × ~90 epochs, 5–6 days, two GTX 580 3GB) are the citable anchor.

---

### F5 — Info — CUDA "release" dating (7:21)

**Claim (paraphrased):** Nvidia introduced the GeForce 8 series and CUDA in 2006; later text refers to CUDA's release in 2006.

**Correction:** CUDA was **announced** November 8, 2006, alongside the G80/GeForce 8800. The public **beta** SDK/toolkit shipped **February 2007**; CUDA 1.0 followed in **mid-2007**. "Announced 2006, available 2007" is the precise formulation. Announcement-date vs. availability-date slippage is harmless in a video and hazardous in a timeline artifact; Lecture 0 should carry both dates.

---

### F6 — Info — Dennard scaling and FinFET framing invert cause and effect (8:49–9:41)

**Claims (paraphrased):** (i) Nvidia raised GPU power draw while simultaneously benefiting from improving per-transistor energy trends, a scaling law called Dennard scaling; (ii) Dennard scaling slowed from 2006 but the trend continues to some extent; (iii) TSMC's 16nm node (Pascal, 2016) was its first with a new transistor type, the FinFET, enabling a large clock jump.

**What's wrong.**

1. **Causality inversion.** Rising TDPs are not a tailwind that happened to coincide with Dennard scaling — they are the industry's **compensation for its collapse**. Dennard's 1974 result: under ideal scaling, power density stays constant, so more transistors at higher clocks came energetically free. That regime broke ~2005–2006 (leakage currents, the voltage-scaling floor). After the break, additional performance must be bought with additional watts. The video presents symptom and disease as two independent gifts.
2. **"Slowed but continues" mischaracterizes.** Dennard scaling *as defined* — constant power density — ended. What continues, at a diminished rate, is energy-per-switching-event improvement from process shrinks. Calling the residual trend "Dennard scaling continuing to some extent" blurs the concept the section exists to teach.
3. **FinFET novelty is scoped-true, globally misleading.** The narration correctly says 16nm was **TSMC's** first FinFET node (16FF/16FF+, volume 2015; Pascal on 16FF+ from May 2016). But Intel shipped tri-gate FinFETs at 22nm in **April 2012** (Ivy Bridge), and Samsung shipped 14nm FinFET in early 2015. A listener will exit believing FinFETs arrived in 2016. The Pascal clock jump itself (~1.2 GHz-class Maxwell boost → ~1.7 GHz-class Pascal boost) is real.

**Correction for reuse:** Dennard scaling (constant power density) broke ~2005–06; post-break, vendors raised power budgets to keep performance scaling; energy/op still improves per node, more slowly. FinFETs: Intel 2012, Samsung early 2015, TSMC volume 2015; Pascal was Nvidia's first TSMC-FinFET generation.

---

### F7 — Info — Chapter title dates GeForce 256 to 1996 (interstitial, ~0:40–1:02)

**Artifact:** The transcript's interstitial section titles (which match YouTube chapter markers) include one dating the GeForce 256 release to 1996. Narration correctly says 1999.

**Correction:** GeForce 256 announced August 31, 1999; retail October 1999. The error is confined to the chapter metadata, not the narration. See R2 for the reviewer's own mislabeling of this artifact in-session.

---

### F8 — Moderate — Parallelism narrated as an Nvidia invention (5:02) — *user-caught*

**Claim:** "The solution Nvidia's engineers found for this bottleneck was parallelism" — followed by the assertion (paraphrased) that they converted the work into a pipeline of sequential steps and then replicated identical pipelines side by side.

**Why this is the most load-bearing error in the script.** The video's structural arc is: bottleneck → Nvidia discovers parallelism → full pipeline on one chip → (much later) that same parallelism turns out to be what neural networks need. If the middle link is invented history, the arc's causality is fiction. And it is invented history, on both halves of the sentence:

**(a) The pipeline abstraction predates Nvidia by 15–20 years.** Nobody at Nvidia converted graphics work into a pipeline of stages; the staged pipeline is the founding abstraction of the discipline. It is standard in the 1970s–80s academic literature and textbooks (Foley & van Dam), and it was codified industrially as the OpenGL fixed-function pipeline in **June 1992** — ten months before Nvidia was founded (April 1993). The field's core noun is *pipeline*. The name is the refutation.

**(b) Parallel graphics hardware is as old as real-time graphics.** Lineage, in order:

- **Evans & Sutherland**, 1970s–80s: real-time image generators for flight simulation exploiting parallel rendering, a full commercial market before consumer 3D existed.
- **Jim Clark, "The Geometry Engine" (SIGGRAPH 1982):** the VLSI geometry processor SGI was founded on; deployed as multiple engines operating in concert in the IRIS geometry subsystem.
- **UNC Pixel-Planes** (Fuchs, Poulton, et al., 1980s onward): literal processor-per-pixel SIMD — 128×128-processor renderer tiles, scaling to six-figure counts of one-bit pixel processors in large Pixel-Planes 5 configurations.
- **SGI RealityEngine** (Akeley, SIGGRAPH 1993): shipping commercial hardware with up to a dozen parallel Geometry Engines feeding hundreds of parallel Image Engines.
- **Consumer space, pre-GeForce, including Nvidia itself:** 3dfx Voodoo (1996) split the pipeline across parallel FBI/TMU chips (3dfx's founders being ex-SGI — the lineage flows through people, not just papers); Voodoo2 SLI (1998) ran two entire cards in scanline interleave; Nvidia's **own RIVA TNT (1998)** — *TwiN Texel* — had two parallel pixel pipelines, the parallelism advertised in the product name; ATI's Rage Fury MAXX (1999) ran dual chips in alternate-frame rendering. The GeForce 256's four pixel pipelines were a **widening of an inherited design**, not a discovery.

**(c) Even the hardware-T&L capstone had prior art.** SGI had geometry in hardware for seventeen years by 1999; 3Dlabs' GLINT Gamma provided geometry processing on professional boards in the late 1990s, before GeForce 256. What GeForce 256 genuinely achieved was **integration and economics**: the full DX7-class pipeline, T&L included, on a single ~17M-transistor die fabbed on TSMC 220nm, at a consumer price point, in consumer volumes. That is a real and important achievement — it is just a different achievement from the one the script narrates.

**(d) The enabling omission.** SGI appears nowhere in the 17-minute script. Erase the lineage and the inheritance reads as invention. This is the purest instance of systemic pattern S1/S2 and the primary reason the video, used uncritically, would contaminate a history-of-GPU-computing artifact.

**Corrected narrative for reuse:** Facing a fill-rate bottleneck, Nvidia widened an already-parallel, already-pipelined design as transistor budgets allowed — scaling a principle the field was built on — and its genuine 1999 contribution was putting the entire pipeline on one cheap die.

---

### F9 — Low — Fixed-function hardware was not "completely wiped away" (7:42)

**Claim (paraphrased):** With CUDA, the fixed-function graphics pipeline was completely eliminated; the GPU ceased to be specialized hardware with dedicated blocks for textures, lighting, and triangles.

**What's wrong.** G80 unified **programmable shading** — the separate vertex/pixel shader units merged into one pool of stream processors. But GPUs retained, and retain today, substantial fixed-function hardware: the rasterizer, ROPs (depth/blend), texture addressing and filtering units, video encode/decode blocks. And the pendulum subsequently swung back toward specialization: tensor cores (Volta, 2017) and RT cores (Turing, 2018) are precisely dedicated-function blocks. For a GPU-computing audience the misstatement is not pedantic — CUDA kernels can and do use the texture units; knowing which silicon is programmable and which is fixed is part of performance work.

**Correction:** 2006 unified the shader stages into general-purpose cores; it did not remove fixed-function hardware, and the 2017+ era re-specialized aggressively.

---

### F10 — Info — "Top-5" metric garbled (10:16)

**Claim (paraphrased):** Pre-2012 hand-engineered models had a 25% error rate among the top-5 performers.

**What's wrong.** "Top-5" modifies the **error metric**, not the leaderboard. Top-5 error = fraction of test images for which the true label is absent from the model's five highest-confidence predictions; it is a property of one model. Actuals: ILSVRC-2010 winner 28.2% top-5, ILSVRC-2011 winner 25.8% top-5 — so the ~25% figure corresponds to the 2011 winner. AlexNet 2012: 15.3% vs. runner-up 26.2%. The video's downstream numbers are right; the metric definition en route is scrambled.

---

### F11 — Info — "World's first GPU" adopted as fact (1:04)

**Claim (paraphrased):** Nvidia called GeForce 256 the world's first GPU, defining the term via single-chip integration of transform, lighting, setup/clipping, and rendering at ≥10M polygons/s; the script notes this smells like marketing, then proceeds on the frame.

**What's wrong.** Two layers. First, the definition is **constructed to have exactly one satisfying element at announcement** — a definitional-marketing move, and the quoted polygon-rate clause is the tell. Second, even the *term* has prior use: Sony's PlayStation (1994) shipped with a rasterizer chip officially named the **GPU** in Sony's own documentation, five years earlier. The script winks at the marketing and then adopts it wholesale; the accurate statement is "first single-chip consumer part integrating the full pipeline including T&L," which is Nvidia's real claim stripped of the coinage theater.

---

### F12 — Info — The 1996 framing erases NV1 and universalizes an industry-wide trait (4:04)

**Claim (paraphrased):** As recently as 1996, Nvidia's cards handled only the render stage, which suited their simple triangle-drawing hardware; everything is triangles.

**What's wrong.** Nvidia's only shipping product in that window was **NV1** (1995, sold as Diamond Edge 3D) — a **quadratic-surface** renderer, explicitly *not* triangle-based, with integrated audio and Sega Saturn pad ports. It failed commercially in large part because Microsoft's Direct3D standardized on triangles; the follow-on NV2 (Sega contract) was cancelled, and Nvidia's triangle pivot is **RIVA 128** (NV3, 1997) — the same part that integrated triangle setup, correctly credited at 4:29. The tidy "triangles all the way down" teleology thus erases the fact that Nvidia's own opening bet was *against* triangles and nearly killed the company. Separately, "render-stage-only in 1996" was not an Nvidia trait but the **entire consumer industry's** state (Voodoo, 1996: rasterization only) — presenting it as a stage in Nvidia's private evolution is the company-centric lens again (S1).

---

### F13 — Info — GeForce 3 shaders "replaced" T&L overstates 2001 (6:54)

**Claim (paraphrased):** Vertex shaders replaced the Transform & Lighting stage; pixel shaders operate per-pixel for rendering.

**What's wrong.** GeForce 3 (2001, DX8) **added** programmable vertex processing (VS 1.1) alongside a fixed-function T&L path that remained exposed and remained what most shipping titles used for years. Pixel-side "shading" in that generation (PS 1.1) was a tightly constrained combiner model, far from general programmability. Replacement — in the sense of fixed-function shading actually disappearing from the programming model — is the 2006/G80 unification, five years later. The two-step story (2001 programmability, 2006 generality) is right; the verb at step one is too strong.

---

## Reviewer errata

**R1 — False recall of video contents (pre-transcript).** Before the transcript was supplied, the reviewer asserted the video covered the Brook/Ian Buck lineage, CUDA's origins, and early HPC verticals (oil & gas, finance). It covers none of these. The recalled outline was cross-contamination from other Nvidia histories (most plausibly Acquired's Part II). Self-identified on first read of the transcript, prior to user challenge. Standing lesson: training-data familiarity with a *topic* is not familiarity with a *document*; provenance labels must be applied before summarizing, not after being caught.

**R2 — Provenance mislabel on the F7 artifact.** The reviewer initially described the erroneous "1996" as an on-screen overlay/caption typo. The interstitial titles in the supplied transcript match YouTube **chapter markers**; the error lives in the chapter metadata. Same defect, wrong location attribution; corrected in F7.

---

## Verified-claims register (survives review)

The following claims were checked and stand. These are safe to inherit, with the noted caveats.

- **GeForce 256:** announced 1999-08-31, retail October 1999; ~17M transistors; fabbed by TSMC (220nm); four pixel pipelines; integrated hardware T&L. Narration's quoted GPU definition matches Nvidia's published marketing definition.
- **GeForce 3:** 2001, first consumer programmable (DX8) shaders. (Verb caveat: F13.)
- **G80 / GeForce 8800 + CUDA announcement:** 2006-11-08. (Availability caveat: F5.)
- **AlexNet:** trained on two GTX 580 **3GB** GPUs; five to six days; ~90 epochs over ~1.2M ILSVRC training images (per the NIPS 2012 paper); top-5 test error **15.3%** vs. runner-up **26.2%** — a 10.9-point margin. Video's "15%, ten points better" is a fair rounding.
- **Human baseline ~5%:** matches Karpathy's measured 5.1% top-5 (2014 self-experiment).
- **ILSVRC first held:** 2010. Winner top-5 errors: 28.2% (2010), 25.8% (2011).
- **AlphaGo d. Lee Sedol:** March 2016, 4–1 in a five-game match. "18-time world champion" is the standard rendering of Lee's 18 international titles.
- **AMD acquired ATI:** 2006 (~$5.4B). **ROCm** launched 2016 and is genuinely open source (the contrast the video draws with CUDA is valid; only the OpenCL sentence is wrong — F1).
- **A100 (GA100):** 54.2B transistors, TSMC 7nm; ~$10–20k street per card in-era; the $33/hr figure is the 8×A100 AWS p4d.24xlarge on-demand rate (unit caveat: F3).
- **Nvidia FY2022 Q3** (quarter ended 2021-10-31, reported 2021-11-17): Gaming **$3.22B, +42% YoY**; Data Center **$2.94B, +55% YoY**. Caveat: the video's "third quarter of 2021" is Nvidia's *fiscal* Q3 (Aug–Oct 2021), which happens to sit close to calendar Q3, so the colloquialism is harmless here but the fiscal offset is worth knowing (Nvidia's FY runs ~one year ahead).
- **"Over 20 billion transistors" for recent GPUs:** GA102 at 28.3B (2020) supports this for a Feb 2022 publication.
- **Dennard breakdown "starting in 2006":** acceptable dating (~2005–06). (Framing caveats: F6.)
- **iPhone analogy facts:** 2007 launch without native third-party apps; App Store July 2008.
- **In-house accelerators:** Google (TPU), Amazon (Inferentia/Trainium), Tesla (FSD computer/Dojo) — correct as named.
- **Cerebras/Graphcore as ASIC entrants:** correct; "multi-million dollar funding rounds" is a large understatement (each had raised well north of $500M by early 2022) but errs only in scale, not kind.
- Not independently anchored, plausible, low stakes: the Not Hotdog dataset figures (3,000 hot dogs / 147,000 non-hot-dogs).

---

## Systemic patterns

**S1 — Company-centric teleology.** Field inheritances narrated as Nvidia inventions or Nvidia-specific stages: parallelism and pipelining (F8), the "first GPU" frame (F11), render-only-1996-as-Nvidia-trait (F12). The story reads as a company discovering the obvious because the field that made it obvious has been cropped out of frame.

**S2 — Lineage erasure.** SGI, UNC, and Evans & Sutherland: absent (enables F8). Princeton/WordNet: absent (F2). Sony's prior use of "GPU": absent (F11). The erasures are what make S1 narratable.

**S3 — Compute-only lens; the memory system does not exist.** No VRAM capacity, no bandwidth, no cache hierarchy appears anywhere in 17 minutes. Consequences: (i) the two-GTX-580 detail floats free — the **3GB** limit that forced AlexNet's two-column, model-parallel topology is unavailable to the narrative, so the single best hardware-shapes-the-science example in the whole story goes untold; (ii) the CPU-slowness explanation reaches for "limited cache" instead of bandwidth, the actual first-order axis; (iii) "processing power linearly proportional to core count" (8:34) holds only when not bandwidth-bound — stated without the roofline caveat that governs real workloads. For a GPU-computing audience, memory is half the plot; this script has none of it.

**S4 — Selective rigor on numbers.** The financials, AlexNet figures, and transistor counts are sourced-quality and check out; the CPU/GPU throughput block (F4) and the rental-rate unit (F3) are rhetoric-grade. The author can clearly source when he chooses to; the register mixes checked and unchecked numbers without marking which is which.

---

## Disposition for the GPU Computing History artifact (Lecture 0)

**Accept (steal freely):**
- The **matrix Chekhov's gun** — planting the matrix-math common denominator inside the graphics-pipeline explainer (2:55) so the AI pivot lands as structural inevitability. Best move in the script.
- The **absorption timeline** as a framing device — render-only → setup on-chip (1997) → full pipeline (1999) → programmable (2001) → general (2006) — *with agency corrected per F8*: Nvidia climbing an industry gradient, not inventing rungs.
- Everything in the verified-claims register, with its caveats.
- The iPhone/App Store platform analogy, attributed as analogy.

**Reject (do not inherit):**
- The parallelism-invention narrative (F8).
- The OpenCL characterization and placement (F1).
- All four numbers in the CPU/GPU throughput block (F4).
- Fixed-function "wiped away" (F9).
- "First GPU" as fact rather than marketing artifact (F11).

**Repair (correct, then use):**
- ImageNet provenance and scale (F2). CUDA dual dating (F5). Dennard/FinFET causality (F6). GeForce 3 verb strength (F13).

**Add (the video's structural gaps, which Lecture 0 should own):**
- The **SGI → consumer** lineage: E&S → Geometry Engine/SGI → Pixel-Planes → RealityEngine → (ex-SGI talent) → 3dfx/consumer boards → GeForce. Parallelism as the field's birthright; Nvidia's contribution as integration + economics ($299, one die, consumer volume).
- The **memory thread**: VRAM capacity and bandwidth as co-protagonists — 3GB GTX 580 → AlexNet's two-stream topology; bandwidth and the roofline as the real CPU/GPU divide; the through-line to why batched-small-matrix work (your migration domain) lives or dies on layout and coalescing.
- The **software connective tissue** the video skips between 2006 and 2012: researchers contorting GEMM into texture operations pre-CUDA; cuda-convnet; and cuDNN (2014) as the moment GPU deep learning stopped requiring heroics.

— End of review —
