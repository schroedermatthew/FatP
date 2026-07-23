// Rebuild of GPU_History_Conclusion_Section_9_Visual.pptx with the prose restored.
// Design system reverse-engineered from the original deck (1280x720 px canvas).
const pptxgen = require('pptxgenjs');
const path = require('path');

const OUT = process.argv[2] || 'GPU_History_Conclusion_Section_9_Visual_v2.pptx';

// ---------------------------------------------------------------- tokens
const BG      = '0B0B10';
const CARD    = '14141D';
const DEEP    = '101018';
const TINT_B  = '111821';
const TINT_G  = '10231F';
const TINT_A  = '211A0D';
const TINT_R  = '211010';
const CHIP_D  = '153A34';
const CHIP_L  = '1D5A4C';
const HAIR    = '2A2A38';
const GHOST   = '23232F';

const AMBER = 'E8A832';
const BLUE  = '63A4FF';
const GREEN = '3CE8B2';
const RED   = 'F27878';
const MID   = '6E84B5';

const WHITE = 'FFFFFF';
const SOFT  = 'D8D8DE';
const MUTED = '858596';
const DIM   = '6E6E82';

const MONO = 'Courier New';
const SANS = 'Calibri';

// ---------------------------------------------------------------- helpers
const I = (v) => v / 96;                       // px -> inches
const B = (x, y, w, h) => ({ x: I(x), y: I(y), w: I(w), h: I(h) });

// Fixed vertical frame, identical on every slide.
const DIAGRAM_TOP = 132, DIAGRAM_BOT = 366;
const PROSE_Y = 380, PROSE_H = 200;
const COL_W = 574, COL_L = 58, COL_R = 648;
const KICK_Y = 596, KICK_H = 56;

function header(slide, title, numeral) {
  slide.addText('SECTION 9  ·  WHY THIS MATTERS FOR YOU',
    { ...B(58, 24, 900, 28), fontFace: MONO, fontSize: 12, bold: true, color: AMBER, margin: 0, valign: 'middle' });
  slide.addText(title,
    { ...B(58, 55, 1080, 62), fontFace: MONO, fontSize: 27, bold: true, color: WHITE, margin: 0, valign: 'middle' });
  slide.addText(String(numeral),
    { ...B(1120, 20, 100, 92), fontFace: MONO, fontSize: 51, bold: true, color: GHOST, margin: 0, align: 'right', valign: 'middle' });
}

function label(slide, text, x, y, w, color, size) {
  slide.addText(text, { ...B(x, y, w, 24), fontFace: MONO, fontSize: size || 11.5, bold: true, color, margin: 0, valign: 'middle' });
}

function card(slide, x, y, w, h, fill, line) {
  slide.addShape('roundRect', {
    ...B(x, y, w, h), fill: { color: fill }, line: { color: line, width: 1 }, rectRadius: 0.05,
  });
}

function chip(slide, x, y, w, h, text, fill, line, color, size) {
  slide.addText(text, {
    shape: 'roundRect', ...B(x, y, w, h), fill: { color: fill }, line: { color: line, width: 1 }, rectRadius: 0.05,
    fontFace: MONO, fontSize: size, bold: true, color, align: 'center', valign: 'middle', margin: 0,
  });
}

function arrow(slide, x, y, w, h, color) {
  slide.addShape('rightArrow', { ...B(x, y, w, h), fill: { color }, line: { color, width: 1 } });
}

// The two prose columns: dim past on the left, lit practice on the right.
function proseColumns(slide, history, practice, note) {
  const BUL = { indent: 13 };
  const body = (items, color) => items.map((t, i) => (
    typeof t === 'string'
      ? { text: t, options: { bullet: BUL, color, breakLine: i < items.length - 1 } }
      : { text: t.text, options: { ...t.opt, bullet: BUL, breakLine: i < items.length - 1 } }
  ));

  card(slide, COL_L, PROSE_Y, COL_W, PROSE_H, DEEP, HAIR);
  slide.addText('THE HISTORY THAT EXPLAINS IT',
    { ...B(COL_L + 24, PROSE_Y + 14, COL_W - 48, 22), fontFace: MONO, fontSize: 11, bold: true, color: MUTED, margin: 0, valign: 'middle' });
  slide.addText(body(history, SOFT), {
    ...B(COL_L + 24, PROSE_Y + 44, COL_W - 48, PROSE_H - (note ? 74 : 58)),
    fontFace: SANS, fontSize: 12, color: SOFT, margin: 0, paraSpaceAfter: 5, lineSpacingMultiple: 0.92, valign: 'top',
  });
  if (note) {
    slide.addText(note, {
      ...B(COL_L + 24, PROSE_Y + PROSE_H - 28, COL_W - 48, 20),
      fontFace: SANS, fontSize: 10.5, bold: true, color: DIM, margin: 0, valign: 'middle',
    });
  }

  card(slide, COL_R, PROSE_Y, COL_W, PROSE_H, CARD, HAIR);
  slide.addText('THE PRACTICE IT DEMANDS',
    { ...B(COL_R + 24, PROSE_Y + 14, COL_W - 48, 22), fontFace: MONO, fontSize: 11, bold: true, color: AMBER, margin: 0, valign: 'middle' });
  slide.addText(body(practice, WHITE), {
    ...B(COL_R + 24, PROSE_Y + 44, COL_W - 48, PROSE_H - 58),
    fontFace: SANS, fontSize: 12, color: WHITE, margin: 0, paraSpaceAfter: 5, lineSpacingMultiple: 0.92, valign: 'top',
  });
}

function kicker(slide, text) {
  slide.addText(text, {
    shape: 'roundRect', ...B(58, KICK_Y, 1164, KICK_H), fill: { color: TINT_A }, line: { color: AMBER, width: 1 },
    rectRadius: 0.05, fontFace: SANS, fontSize: 15, bold: true, color: AMBER, align: 'center', valign: 'middle',
    margin: 0,
  });
}

// ---------------------------------------------------------------- deck
const pres = new pptxgen();
pres.layout = 'LAYOUT_WIDE';           // 13.333 x 7.5 in == 1280 x 720 px
pres.author = 'GPU Computing History';
pres.title = 'Section 9 - Why This Matters For You';

const newSlide = (title, numeral) => {
  const s = pres.addSlide();
  s.background = { color: BG };
  header(s, title, numeral);
  return s;
};

// ================================================================ SLIDE 1
{
  const s = newSlide('It is a throughput machine', 1);

  label(s, 'CPU  ·  FAST PER TASK', 80, DIAGRAM_TOP + 4, 400, BLUE);
  label(s, 'GPU  ·  FAST IN AGGREGATE', 600, DIAGRAM_TOP + 4, 500, GREEN);

  card(s, 110, 166, 270, 160, CARD, BLUE);
  s.addText('one hard problem\nas quickly as possible', {
    ...B(130, 186, 230, 76), fontFace: SANS, fontSize: 15, bold: true, color: WHITE,
    align: 'center', valign: 'middle', margin: 0, lineSpacingMultiple: 1.05,
  });
  s.addText('latency', {
    ...B(110, 276, 270, 30), fontFace: MONO, fontSize: 13, color: MUTED, align: 'center', valign: 'middle', margin: 0,
  });

  arrow(s, 452, 226, 92, 40, GHOST);

  const cols = 13, rows = 4, cell = 34, gap = 8, gx = 600, gy = 166;
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      const lit = (r * 5 + c * 3) % 7 === 0;
      s.addShape('roundRect', {
        ...B(gx + c * (cell + gap), gy + r * (cell + gap), cell, cell),
        fill: { color: lit ? CHIP_L : CHIP_D },
        line: lit ? { color: GREEN, width: 1 } : { color: CHIP_D, width: 1 },
        rectRadius: 0.06,
      });
    }
  }
  s.addText('millions of independent pixels made latency irrelevant', {
    ...B(gx, 336, cols * (cell + gap) - gap, 28), fontFace: SANS, fontSize: 13.5, color: GREEN, margin: 0, valign: 'middle',
  });

  proseColumns(s,
    [
      'Gaming economics built it — millions of $300 cards, not thousands of $100k workstations.',
      'A frame is millions of independent pixels, so latency on any one never mattered. The silicon inherited that.',
      'CUDA changed the language, not the economics: compute rides hardware that games paid for.',
    ],
    [
      'Ask whether there is enough independent work to be worth the trip — not how fast the card is.',
      'Small or serial work often loses, sometimes by orders of magnitude.',
      'Throughput-over-latency is not a setting you choose. It is the machine you were given.',
    ],
    '→  §2 · THE VOLUME FLYWHEEL');

  kicker(s, 'The machine is a consequence of who paid for it. Program the economics, not the brochure.');
  s.addNotes('Gaming economics built a machine optimized for aggregate throughput: millions of inexpensive cards drawing millions of independent pixels. The audience has already seen that history. Emphasize that CUDA did not change the economics; compute inherited the architecture shaped by games. Small or serial work often loses, sometimes by orders of magnitude. The first question is not how fast the GPU is, but whether the work exposes enough parallelism.');
}

// ================================================================ SLIDE 2
{
  const s = newSlide('The programming model is the hardware', 2);

  label(s, 'WRITE ONCE', 78, DIAGRAM_TOP + 4, 330, AMBER);
  card(s, 78, 166, 360, 150, DEEP, AMBER);
  s.addText('kernel update(i)\n{\n    y[i] = a*x[i] + y[i];\n}', {
    ...B(102, 182, 320, 120), fontFace: MONO, fontSize: 14.5, color: WHITE, margin: 0, lineSpacingMultiple: 1.15,
  });
  s.addText('one function', {
    ...B(78, 324, 360, 28), fontFace: SANS, fontSize: 13.5, bold: true, color: AMBER, margin: 0, valign: 'middle',
  });

  arrow(s, 458, 221, 90, 40, AMBER);

  label(s, 'RUN EVERYWHERE', 590, DIAGRAM_TOP + 4, 520, GREEN);
  const cols = 10, rows = 4, cw = 48, ch = 30, gp = 6, gx = 590, gy = 166;
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      const i = r * cols + c;
      const lit = i % 7 === 0;
      if (lit) {
        chip(s, gx + c * (cw + gp), gy + r * (ch + gp), cw, ch, 'i' + i, CHIP_L, GREEN, GREEN, 9.5);
      } else {
        s.addShape('roundRect', {
          ...B(gx + c * (cw + gp), gy + r * (ch + gp), cw, ch),
          fill: { color: CARD }, line: { color: HAIR, width: 1 }, rectRadius: 0.06,
        });
      }
    }
  }
  s.addText('many independent elements', {
    ...B(gx, 324, cols * (cw + gp) - gp, 28), fontFace: SANS, fontSize: 13.5, bold: true, color: GREEN, margin: 0, valign: 'middle',
  });

  proseColumns(s,
    [
      { text: 'graphics disguise — floats as pixel colours, matrices as textures', opt: { color: MUTED } },
      { text: 'Brook, 2004 — streams and kernels; the model finally gets a name', opt: { color: BLUE } },
      { text: 'CUDA, 2006 — the same model, with the disguise removed', opt: { color: GREEN } },
    ],
    [
      'Data-parallel code often ports almost by transcription: one kernel, many elements, no cross-talk.',
      'Deeply serial code must be rethought, not translated. If element i needs i−1, you fight the machine.',
      'Ask of every loop: are the iterations strangers to each other?',
    ],
    '→  §4 NOTE · MATRIX ADDITION AS PIXELS');

  kicker(s, 'CUDA did not invent the model — it stopped requiring the disguise.');
  s.addNotes('The model was present before CUDA: every pixel was already decided independently. Section 4 showed floats disguised as colors and matrices as textures. Brook named the stream-and-kernel abstraction; CUDA removed the graphics disguise and made it native. Data-parallel code often ports naturally. Deeply serial code must be rethought rather than translated.');
}

// ================================================================ SLIDE 3
{
  const s = newSlide('Call a library first', 3);

  label(s, 'YOUR CODE', 80, DIAGRAM_TOP + 4, 240, WHITE);
  chip(s, 80, 166, 260, 84, 'A · B', CARD, WHITE, WHITE, 22);
  chip(s, 80, 266, 260, 84, 'FFT(x)', CARD, WHITE, WHITE, 22);

  arrow(s, 352, 193, 90, 30, GHOST);
  arrow(s, 352, 293, 90, 30, GHOST);

  label(s, 'OPERATION CONTRACT', 455, DIAGRAM_TOP + 4, 280, AMBER);
  chip(s, 455, 166, 260, 84, 'cuBLAS', TINT_A, AMBER, AMBER, 20);
  chip(s, 455, 266, 260, 84, 'cuFFT', TINT_A, AMBER, AMBER, 20);

  arrow(s, 727, 193, 90, 30, GHOST);
  arrow(s, 727, 293, 90, 30, GHOST);

  label(s, 'CHANGING SILICON', 830, DIAGRAM_TOP + 4, 330, GREEN);
  chip(s, 830, 166, 320, 52, 'Volta', CARD, GREEN, GREEN, 16);
  chip(s, 830, 228, 320, 52, 'Ampere', CARD, GREEN, GREEN, 16);
  chip(s, 830, 290, 320, 60, 'Hopper', CHIP_D, GREEN, GREEN, 16);

  proseColumns(s,
    [
      "Section 5's bet: the integrated stack — CUDA plus tuned libraries — beat the spec that shipped alone.",
      'cuBLAS, cuFFT, cuSOLVER, cuDNN: kernels retuned per architecture by the people who drew the silicon.',
      'When tensor cores arrived in 2017, updated libraries could expose them with little or no code change.',
    ],
    [
      'On common operations, hand-written kernels often lose to vendor libraries, and may age worse.',
      'Write to the operation, not the device: say multiply these matrices, and let the library pick the path.',
      'Your own kernels are for the glue between library calls — not the linear algebra inside them.',
    ],
    '→  §7 NOTE · THE RUNWAY AND THE PLANE');

  kicker(s, 'The library is the contract that can outlive the hardware — and the usual door to the tensor cores.');
  s.addNotes("NVIDIA's integrated stack—CUDA plus tuned libraries—became part of the product. Vendor libraries are rewritten for each architecture and are the usual path to specialized units such as tensor cores. On common operations, hand-written kernels often lose to vendor libraries and may age worse across hardware generations. Do not make the claim absolute: custom kernels remain important for fusion, unusual operations, and the glue between library calls.");
}

// ================================================================ SLIDE 4
{
  const s = newSlide('Moving data is the hidden cost', 4);

  label(s, 'THE EXPENSIVE CROSSING  ·  transfer → compute → transfer', 78, DIAGRAM_TOP + 2, 760, RED);
  chip(s, 80, 162, 215, 80, 'CPU\nMEMORY', CARD, BLUE, BLUE, 17);
  arrow(s, 309, 186, 140, 32, RED);
  chip(s, 461, 162, 215, 80, 'GPU\nKERNEL', TINT_R, RED, RED, 17);
  arrow(s, 690, 186, 140, 32, RED);
  chip(s, 842, 162, 215, 80, 'CPU\nMEMORY', CARD, BLUE, BLUE, 17);
  s.addText('transfer', { ...B(309, 220, 140, 20), fontFace: MONO, fontSize: 10, bold: true, color: RED, align: 'center', margin: 0 });
  s.addText('transfer', { ...B(690, 220, 140, 20), fontFace: MONO, fontSize: 10, bold: true, color: RED, align: 'center', margin: 0 });

  label(s, 'THE RESIDENT LOOP  ·  upload once, then compute', 78, 262, 760, GREEN);
  chip(s, 80, 292, 215, 76, 'UPLOAD\nONCE', CARD, BLUE, BLUE, 16);
  arrow(s, 309, 316, 60, 28, GREEN);
  card(s, 387, 292, 670, 76, TINT_G, GREEN);
  const kx = [409, 575, 741, 907];
  kx.forEach((x, i) => {
    chip(s, x, 310, 132, 40, 'K' + (i + 1), CHIP_D, GREEN, GREEN, 15);
    if (i < 3) arrow(s, x + 141, 320, 24, 20, GREEN);
  });

  proseColumns(s,
    [
      "The GPU is a coprocessor across a bus. Section 6's skeptics said so, and were right; it was not fatal.",
      'Tianhe-1A and Titan won by keeping data resident on the device, not shuttling it per operation.',
      'Matmul is the exception that proves it: n³ arithmetic on n² data is why it could be frozen (§8).',
    ],
    [
      'Arithmetic per byte moved must pay for the move. Count it before you port — the roofline question.',
      'Keep data resident: chain kernels on the device. The worst pattern is transfer–compute–transfer.',
      'A slow port is often a transfer problem in a compute costume — but check occupancy and divergence.',
    ]);

  kicker(s, 'The bus never got fast enough to stop mattering. Every fast GPU program is a data-residency design first.');
  s.addNotes('The GPU remains a coprocessor across a bus. The HPC skeptics were right that movement could erase the arithmetic win; it simply was not fatal when workloads kept data resident and did enough work per byte. Use this slide to introduce arithmetic intensity and the roofline vocabulary. A slow GPU port may be a transfer problem, but also acknowledge occupancy, divergence, synchronization, and insufficient parallelism.');
}

// ================================================================ SLIDE 5
{
  const s = newSlide('Small costs scale; precision is priced', 5);

  label(s, 'LAUNCH GRANULARITY', 78, DIAGRAM_TOP + 4, 500, AMBER);
  s.addText('1000 tiny launches', { ...B(80, 168, 300, 22), fontFace: MONO, fontSize: 11.5, color: MUTED, margin: 0, valign: 'middle' });
  for (let i = 0; i < 14; i++) {
    s.addShape('roundRect', { ...B(80 + i * 20, 200, 14, 14), fill: { color: RED }, line: { color: RED, width: 1 }, rectRadius: 0.1 });
  }
  arrow(s, 372, 194, 60, 26, MUTED);
  chip(s, 448, 182, 170, 60, 'BATCH\n+ FUSE', CHIP_D, GREEN, GREEN, 15);
  s.addText('Do more per launch.', { ...B(80, 300, 540, 30), fontFace: SANS, fontSize: 15, bold: true, color: WHITE, margin: 0, valign: 'middle' });

  s.addShape('line', { ...B(648, 148, 0, 200), line: { color: HAIR, width: 1 } });

  label(s, 'PRECISION CHANGES THE BILL', 676, DIAGRAM_TOP + 4, 500, AMBER);
  const bars = [
    ['FP64', 440, '8 bytes', BLUE],
    ['FP32', 220, '4 bytes', MID],
    ['FP16', 110, '2 bytes', GREEN],
    ['INT8', 55, '1 byte', AMBER],
  ];
  bars.forEach(([name, w, bytes, col], i) => {
    const y = 172 + i * 42;
    s.addText(name, { ...B(676, y, 64, 28), fontFace: MONO, fontSize: 12.5, bold: true, color: col, margin: 0, valign: 'middle' });
    s.addShape('roundRect', { ...B(748, y, w, 28), fill: { color: col }, line: { color: col, width: 1 }, rectRadius: 0.08 });
    // Narrow bars cannot hold their own label — set it outside, in the bar's colour.
    if (w >= 100) {
      s.addText(bytes, { ...B(758, y, w - 20, 28), fontFace: MONO, fontSize: 10.5, bold: true, color: BG, margin: 0, valign: 'middle' });
    } else {
      s.addText(bytes, { ...B(748 + w + 10, y, 100, 28), fontFace: MONO, fontSize: 10.5, bold: true, color: col, margin: 0, valign: 'middle' });
    }
  });

  proseColumns(s,
    [
      'Every launch costs microseconds of host round-trip — invisible once, ruinous a million times.',
      'The fixes are structural: batch the work, fuse the kernels, capture and replay the graph (CUDA Graphs).',
      'Tensor cores opened fast low-precision paths — FP16, INT8, INT4 — precision became a priced spectrum.',
    ],
    [
      'Halve the width and the same traffic carries twice the values — precision is a data-movement dial first.',
      'Do not read the format list as a universal ranking. The device and the workload decide.',
      'Batch and fuse before you tune anything else; launch count is usually the cheaper win.',
    ]);

  kicker(s, 'Choose precision like you choose an algorithm: deliberately, and check what the actual card provides.');
  s.addNotes('First, every launch has overhead. The structural remedies are batching, fusion, and capture/replay for repeated sequences. Second, precision is a priced choice. The history showed consumer FP64 deliberately segmented from datacenter FP64, while tensor cores created fast lower-precision paths. Precision also changes data movement: halving width doubles the values carried by the same memory traffic. Avoid treating the displayed formats as a universal ranking; the real device and workload decide.');
}

// ================================================================ SLIDE 6  (restored: the FP64 card-class point)
{
  const s = newSlide('Lesson 5, continued — the card class decides', 5);

  label(s, 'FP64 THROUGHPUT, AS A FRACTION OF FP32', 78, DIAGRAM_TOP + 4, 560, AMBER);
  const ratios = [
    ['DATACENTER', 300, '≈ 1:2', BLUE, ''],
    ['CONSUMER', 75, '1:8', MID, 'earlier generations'],
    ['', 19, '1:32', MID, ''],
    ['', 10, '1:64', RED, 'same generation, different market'],
  ];
  ratios.forEach(([name, w, tag, col, note], i) => {
    const y = 174 + i * 44;
    if (name) s.addText(name, { ...B(78, y, 150, 26), fontFace: MONO, fontSize: 11.5, bold: true, color: col, margin: 0, valign: 'middle' });
    s.addShape('roundRect', { ...B(236, y, w, 26), fill: { color: col }, line: { color: col, width: 1 }, rectRadius: 0.08 });
    s.addText(tag, { ...B(236 + w + 12, y, 90, 26), fontFace: MONO, fontSize: 11.5, bold: true, color: col, margin: 0, valign: 'middle' });
    if (note) s.addText(note, { ...B(236 + w + 74, y, 300, 26), fontFace: SANS, fontSize: 11, color: MUTED, margin: 0, valign: 'middle' });
  });

  s.addShape('line', { ...B(660, 148, 0, 200), line: { color: HAIR, width: 1 } });

  label(s, '16-BIT IS A MEMORY STORY', 700, DIAGRAM_TOP + 4, 500, GREEN);
  s.addText('FP32', { ...B(700, 180, 60, 24), fontFace: MONO, fontSize: 11.5, bold: true, color: MID, margin: 0, valign: 'middle' });
  for (let i = 0; i < 6; i++) {
    s.addShape('roundRect', { ...B(700 + i * 80, 208, 74, 34), fill: { color: MID }, line: { color: MID, width: 1 }, rectRadius: 0.08 });
  }
  s.addText('FP16', { ...B(700, 252, 60, 24), fontFace: MONO, fontSize: 11.5, bold: true, color: GREEN, margin: 0, valign: 'middle' });
  for (let i = 0; i < 12; i++) {
    s.addShape('roundRect', { ...B(700 + i * 40, 280, 34, 34), fill: { color: GREEN }, line: { color: GREEN, width: 1 }, rectRadius: 0.08 });
  }
  s.addText('one fetch, the same bytes — twice the values', {
    ...B(700, 326, 480, 26), fontFace: SANS, fontSize: 12.5, color: GREEN, margin: 0, valign: 'middle',
  });

  proseColumns(s,
    [
      'FP64 on consumer cards was segmented deliberately — 1:8, then 1:24, 1:32, 1:64 of the FP32 rate.',
      'A business decision, not physics (Section 6). Datacenter parts kept FP64 near half the FP32 rate.',
      'The memory side moved the other way: narrower formats to carry more values per fetch.',
    ],
    [
      'If FP64 carries your work — structural dynamics, covariance solves, ill-conditioning — the card class is the first decision.',
      'Memory-bound? Cut bytes first, then recover accuracy where appropriate by iterative refinement.',
      'Check the actual ratio on the actual part. One line of the spec sheet decides the design.',
    ],
    '→  §6 · THE FP64 MARKET SEGMENT');

  kicker(s, 'Precision is priced twice — once in silicon, once in bytes. Read both price lists before you design.');
  s.addNotes('This slide restores the card-class argument. GPUs are not one machine: many datacenter parts run double precision near half the FP32 rate, while consumer cards of the same generation may be limited to 1:32 or 1:64. That gap is a market decision, not physics. If FP64 dominates the workload, the product class is the first architectural decision, before any kernel is written. The right-hand panel makes the second, separate point: narrower formats are a data-movement win before they are an arithmetic win, because a bandwidth-limited kernel gets roughly twice the values from the same fetch. Iterative refinement is the standard way to buy accuracy back after cutting bytes.');
}

// ================================================================ SLIDE 7
{
  const s = newSlide('Read the machine before you port', 6);

  s.addText('WHERE IS THE KERNEL ON THE ROOFLINE?', {
    shape: 'roundRect', ...B(420, 132, 440, 56), fill: { color: TINT_A }, line: { color: AMBER, width: 1 }, rectRadius: 0.06,
    fontFace: MONO, fontSize: 14.5, bold: true, color: AMBER, align: 'center', valign: 'middle', margin: 0,
  });

  label(s, 'MEMORY-BOUND', 95, 218, 310, GREEN, 12.5);
  card(s, 80, 248, 320, 142, TINT_G, GREEN);
  s.addText('CUT BYTES', { ...B(108, 266, 264, 34), fontFace: MONO, fontSize: 18, bold: true, color: GREEN, margin: 0, valign: 'middle' });
  s.addText('lower precision\nkeep data resident\nfuse nearby work', {
    ...B(108, 306, 264, 74), fontFace: SANS, fontSize: 13.5, color: WHITE, margin: 0, lineSpacingMultiple: 1.05,
  });

  label(s, 'COMPUTE-BOUND', 895, 218, 310, BLUE, 12.5);
  card(s, 880, 248, 320, 142, TINT_B, BLUE);
  s.addText('MATCH THE MATH', { ...B(908, 266, 264, 34), fontFace: MONO, fontSize: 18, bold: true, color: BLUE, margin: 0, valign: 'middle' });
  s.addText('library operation\nhardware fast path\nformat support', {
    ...B(908, 306, 264, 74), fontFace: SANS, fontSize: 13.5, color: WHITE, margin: 0, lineSpacingMultiple: 1.05,
  });

  card(s, 430, 248, 420, 142, CARD, HAIR);
  s.addText('THE HISTORY GIVES\nYOU THE QUESTIONS', {
    ...B(450, 268, 380, 62), fontFace: MONO, fontSize: 18, bold: true, color: WHITE, align: 'center', valign: 'middle', margin: 0, lineSpacingMultiple: 1.05,
  });
  s.addText('parallelism · movement · launches · specialization · precision', {
    ...B(440, 340, 400, 34), fontFace: SANS, fontSize: 11.5, color: MUTED, align: 'center', valign: 'middle', margin: 0,
  });

  label(s, 'FP64-DOMINATED?', 95, 420, 310, GREEN, 12.5);
  card(s, 80, 450, 320, 118, CARD, GREEN);
  s.addText('Check the ratio.\nChoose the product class.', {
    ...B(108, 466, 264, 86), fontFace: SANS, fontSize: 14, bold: true, color: WHITE, margin: 0, valign: 'middle', lineSpacingMultiple: 1.1,
  });

  label(s, 'COMMON OPERATION?', 895, 420, 310, BLUE, 12.5);
  card(s, 880, 450, 320, 118, CARD, BLUE);
  s.addText('Call the library first.\nWrite custom glue second.', {
    ...B(908, 466, 264, 86), fontFace: SANS, fontSize: 14, bold: true, color: WHITE, margin: 0, valign: 'middle', lineSpacingMultiple: 1.1,
  });

  card(s, 430, 450, 420, 118, TINT_A, AMBER);
  s.addText('MEASURE\nBEFORE YOU MOVE', {
    ...B(450, 466, 380, 86), fontFace: MONO, fontSize: 17, bold: true, color: AMBER, align: 'center', valign: 'middle', margin: 0, lineSpacingMultiple: 1.1,
  });

  // Connectors last, so the cards cannot paint over them.
  arrow(s, 403, 309, 24, 20, GREEN);
  arrow(s, 853, 309, 24, 20, BLUE);
  arrow(s, 403, 499, 24, 20, GREEN);
  arrow(s, 853, 499, 24, 20, BLUE);

  kicker(s, 'The history stops here. The porting conversation starts with better questions.');
  s.addNotes('This is the handoff to the technical porting discussions. The goal is not a universal recipe; it is a disciplined first set of questions. Locate the kernel on the roofline. If memory-bound, reduce bytes and preserve residency. If compute-bound, match the mathematical operation and supported formats to the hardware path. If FP64 dominates, inspect the actual device ratio before choosing hardware. If the operation is common, start with the library. Measure before offloading.');
}

pres.writeFile({ fileName: OUT }).then(() => console.log('wrote', path.resolve(OUT)));
