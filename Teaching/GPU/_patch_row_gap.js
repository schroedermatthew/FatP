const fs = require('fs');
const path = 'C:/Users/mtthw/Desktop/AI Projects/FatP/Teaching/GPU/GPU_Computing_History.html';
const lines = fs.readFileSync(path, 'utf8').split('\n');

function patchPipeline(line) {
  let s = line;

  // Row 2 blocks: move up so gap row1→row2 = 52px (row1 bottom 145, row2 top 197)
  s = s.replace(/(<g class="shader-stage" data-stage="rasterizer"[\s\S]*?<rect x="42" )y="245"/, '$1y="197"');
  s = s.replace(/(<g class="shader-stage" data-stage="depthBlend"[\s\S]*?<rect x="592" )y="245"/, '$1y="197"');
  s = s.replace(/(<g class="shader-stage" data-stage="(?:textureCombine|pixelShader)"[\s\S]*?<rect x="292" )y="230"/, '$1y="197"');

  // Row 2 labels (rect y + 34 / +59 / +84)
  s = s.replace(/(data-stage="rasterizer"[\s\S]*?<text x="150" )y="279"/, '$1y="231"');
  s = s.replace(/(data-stage="rasterizer"[\s\S]*?<text x="150" y="231" class="shader-label">Rasterizer<\/text><text x="150" )y="304"/, '$1y="256"');

  s = s.replace(/(data-stage="depthBlend"[\s\S]*?<text x="725" )y="279"/, '$1y="231"');
  s = s.replace(/(data-stage="depthBlend"[\s\S]*?<text x="725" y="231" class="shader-label">Depth \/ blend \/ ROP<\/text><text x="725" )y="304"/, '$1y="256"');

  // Middle block labels
  s = s.replace(/(data-stage="textureCombine"[\s\S]*?<text x="425" )y="264"/, '$1y="231"');
  s = s.replace(/(data-stage="textureCombine"[\s\S]*?<text x="425" y="231" class="shader-label">Texture \/ combine<\/text><text x="425" )y="289"/, '$1y="256"');
  s = s.replace(/y="undefined" class="shader-sub">and combiner network/, 'y="281" class="shader-sub">and combiner network');

  s = s.replace(/(data-stage="pixelShader"[\s\S]*?<text x="425" )y="264"/, '$1y="231"');
  s = s.replace(/(data-stage="pixelShader"[\s\S]*?<text x="425" y="231" class="shader-label">Pixel shader<\/text><text x="425" )y="289"/, '$1y="256"');
  s = s.replace(/y="undefined" class="shader-sub">texture shaders \+ combiners/, 'y="281" class="shader-sub">texture shaders + combiners');

  // Fix fixed T&L third line while we're here
  s = s.replace(/(data-stage="fixedTl"[\s\S]*?)y="undefined" class="shader-sub">one model baked in/, '$1y="129" class="shader-sub">one model baked in');

  // L-path: horizontal segment centered between row1 bottom (145) and row2 top (197)
  s = s.replace(/M725 135 V 218 H 150 V 245/g, 'M725 135 V 171 H 150 V 197');

  // Row 2 horizontal arrows
  s = s.replace(/M258 285 H 292/g, 'M258 245 H 292');
  s = s.replace(/M558 285 H 592/g, 'M558 245 H 592');

  // Depth → framebuffer
  s = s.replace(/M725 325 V 400/g, 'M725 277 V 400');

  return s;
}

let changed = 0;
for (let i = 0; i < lines.length; i++) {
  if (lines[i].includes('data-pipeline="p2"') || lines[i].includes('data-pipeline="p3"')) {
    const next = patchPipeline(lines[i]);
    if (next !== lines[i]) { lines[i] = next; changed++; }
  }
}

fs.writeFileSync(path, lines.join('\n'));
console.log('patched pipelines:', changed);