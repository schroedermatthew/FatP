const fs = require('fs');
const path = 'C:/Users/mtthw/Desktop/AI Projects/FatP/Teaching/GPU/GPU_Computing_History.html';
const lines = fs.readFileSync(path, 'utf8').split('\n');

const H = 80;
const Y1 = 55;
const Y2 = 197;
const FB_Y = 345;
const CORNER_Y = 166; // midpoint row1 bottom 135 → row2 top 197

function patch(line) {
  let s = line;

  // Row 1: fixed T&L / tall blocks → h=80, y=55, 3-line text
  s = s.replace(
    /(<g class="shader-stage" data-stage="fixedTl"[\s\S]*?<rect x="292" )y="45"( width="246" )height="100"/,
    `$1y="${Y1}"$2height="${H}"`
  );
  s = s.replace(
    /(data-stage="fixedTl"[\s\S]*?<text x="415" )y="79"/,
    '$1y="84"'
  );
  s = s.replace(
    /(data-stage="fixedTl"[\s\S]*?<text x="415" y="84" class="shader-label">Fixed T&amp;L<\/text><text x="415" )y="104"/,
    '$1y="103"'
  );
  s = s.replace(
    /(data-stage="fixedTl"[\s\S]*?<text x="415" y="103" class="shader-sub">transform and lighting<\/text><text x="415" )y="129"/,
    '$1y="122"'
  );

  // Row 2: texture / pixel shader h=110 → h=80, 3-line text
  s = s.replace(
    /(<g class="shader-stage[^"]*" data-stage="(?:textureCombine|pixelShader)"[\s\S]*?<rect x="292" y="197" width="266" )height="110"/,
    `$1height="${H}"`
  );
  s = s.replace(
    /(data-stage="(?:textureCombine|pixelShader)"[\s\S]*?<text x="425" )y="231"( class="shader-label">(?:Texture \/ combine|Pixel shader)<\/text><text x="425" )y="256"/,
    '$1y="226"$2y="245"'
  );
  s = s.replace(
    /(data-stage="(?:textureCombine|pixelShader)"[\s\S]*?<text x="425" y="245" class="shader-sub">[^<]*<\/text><text x="425" )y="281"/,
    '$1y="264"'
  );

  // Framebuffer h=72 → h=80
  s = s.replace(
    /(data-stage="framebuffer"[\s\S]*?<rect x="592" y="345" width="266" )height="72"/,
    `$1height="${H}"`
  );

  // Centered L-path horizontal between row 1 and 2
  s = s.replace(/M725 135 V 171 H 150 V 197/g, `M725 135 V ${CORNER_Y} H 150 V ${Y2}`);

  return s;
}

let n = 0;
for (let i = 0; i < lines.length; i++) {
  if (lines[i].includes('data-pipeline="p2"') || lines[i].includes('data-pipeline="p3"')) {
    lines[i] = patch(lines[i]);
    n++;
  }
}

fs.writeFileSync(path, lines.join('\n'));
console.log('uniform blocks applied to', n, 'pipelines');