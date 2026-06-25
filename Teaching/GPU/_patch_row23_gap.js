const fs = require('fs');
const path = 'C:/Users/mtthw/Desktop/AI Projects/FatP/Teaching/GPU/GPU_Computing_History.html';
const lines = fs.readFileSync(path, 'utf8').split('\n');

const FB_Y = 345; // was 400; row2 bottom 307 → 38px gap

function patch(line, pipeline) {
  let s = line;

  // Framebuffer block
  s = s.replace(
    /(data-stage="framebuffer"[\s\S]*?<rect x="592" )y="400"/,
    `$1y="${FB_Y}"`
  );
  s = s.replace(
    /(data-stage="framebuffer"[\s\S]*?<text x="725" )y="434"/,
    `$1y="${FB_Y + 34}"`
  );
  s = s.replace(
    /(data-stage="framebuffer"[\s\S]*?<text x="725" y="\d+" class="shader-label">Framebuffer<\/text><text x="725" )y="459"/,
    `$1y="${FB_Y + 59}"`
  );

  // Depth → framebuffer arrow
  s = s.replace(/M725 277 V 400/g, `M725 277 V ${FB_Y}`);

  if (pipeline === 'p2') {
    s = s.replace(/<text x="62" y="434" class="shader-note-text">/, `<text x="62" y="${FB_Y + 34}" class="shader-note-text">`);
    s = s.replace(/<text x="62" y="458" class="shader-note-text">/, `<text x="62" y="${FB_Y + 58}" class="shader-note-text">`);
  }

  if (pipeline === 'p3') {
    s = s.replace(/<text x="62" y="414" class="shader-note-text">/, `<text x="62" y="${FB_Y + 14}" class="shader-note-text">`);
    s = s.replace(/<text x="62" y="434" class="shader-note-text">/, `<text x="62" y="${FB_Y + 34}" class="shader-note-text">`);
    s = s.replace(/<text x="62" y="454" class="shader-note-text">/, `<text x="62" y="${FB_Y + 54}" class="shader-note-text">`);
    s = s.replace(/<text x="62" y="474" class="shader-note-text">/, `<text x="62" y="${FB_Y + 74}" class="shader-note-text">`);
  }

  return s;
}

let n = 0;
for (let i = 0; i < lines.length; i++) {
  if (lines[i].includes('data-pipeline="p2"')) {
    lines[i] = patch(lines[i], 'p2');
    n++;
  } else if (lines[i].includes('data-pipeline="p3"')) {
    lines[i] = patch(lines[i], 'p3');
    n++;
  }
}

fs.writeFileSync(path, lines.join('\n'));
console.log('patched', n, 'pipelines; framebuffer y=', FB_Y);