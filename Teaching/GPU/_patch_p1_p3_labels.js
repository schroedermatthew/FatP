const fs = require('fs');
const path = 'C:/Users/mtthw/Desktop/AI Projects/FatP/Teaching/GPU/GPU_Computing_History.html';
const lines = fs.readFileSync(path, 'utf8').split('\n');

function patchVertexDataCpu(line, rectPattern) {
  return line
    .replace(
      /<g class="shader-stage" data-stage="vertexData"/,
      '<g class="shader-stage cpu" data-stage="vertexData"'
    )
    .replace(rectPattern, (m) => m.replace('class="shader-box"', 'class="shader-box cpu"'));
}

for (let i = 0; i < lines.length; i++) {
  if (lines[i].includes('data-pipeline="p1"')) {
    lines[i] = patchVertexDataCpu(
      lines[i],
      /<rect x="42" y="45" width="196" height="68" rx="12" class="shader-box">/
    );
  } else if (lines[i].includes('data-pipeline="p3"')) {
    lines[i] = patchVertexDataCpu(
      lines[i],
      /<rect x="42" y="55" width="216" height="80" rx="12" class="shader-box">/
    );
    lines[i] = lines[i].replace(
      '<text x="250" y="36" class="shader-stage-title">Per-vertex work</text>',
      '<text x="415" y="36" class="shader-stage-title">Per-vertex work</text>'
    );
  }
}

fs.writeFileSync(path, lines.join('\n'));
console.log('done');