const fs = require('fs');
const path = 'C:/Users/mtthw/Desktop/AI Projects/FatP/Teaching/GPU/GPU_Computing_History.html';
let h = fs.readFileSync(path, 'utf8');

const lineIdx = h.split('\n').findIndex(l => l.includes('data-pipeline="p2"'));
if (lineIdx < 0) { console.error('p2 not found'); process.exit(1); }
let line = h.split('\n')[lineIdx];

const arrowsBlock =
  '<g class="shader-arrows">' +
  '<path d="M258 100 H 292" class="shader-gpu-transfer"></path>' +
  '<path d="M538 100 H 592" class="shader-arrow"></path>' +
  '<path d="M725 135 V 218 H 150 V 245" class="shader-line"></path>' +
  '<path d="M258 285 H 292" class="shader-arrow"></path>' +
  '<path d="M558 285 H 592" class="shader-arrow"></path>' +
  '<path d="M725 325 V 400" class="shader-arrow"></path>' +
  '</g>';

// Remove arrows from top (they were painting under boxes)
line = line.replace(
  /<g class="shader-arrows">[\s\S]*?<\/g>/,
  ''
);
// Remove stray top layer if re-running
line = line.replace(/<g class="shader-arrows-top">[\s\S]*?<\/g>/, '');

// Move note text into vertical down-arrow corridor
line = line.replace(
  '<text x="168" y="176" class="shader-note-text">By 1999 the consumer GPU absorbed the whole pipeline.</text><text x="168" y="200" class="shader-note-text">Every stage was frozen in silicon &ndash; fast, but fixed.</text>',
  '<text x="168" y="212" class="shader-note-text">By 1999 the consumer GPU absorbed the whole pipeline.</text><text x="168" y="232" class="shader-note-text">Every stage was frozen in silicon &ndash; fast, but fixed.</text>'
);

// Paint arrows on top of all boxes
line = line.replace('</svg><div class="shader-detail"', arrowsBlock + '</svg><div class="shader-detail"');

const lines = h.split('\n');
lines[lineIdx] = line;
fs.writeFileSync(path, lines.join('\n'));
console.log('p2 fixed');