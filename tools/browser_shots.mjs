// Runs each sample page of a browser build in headless Chromium (or Firefox) and saves its canvas as a PNG.
//
//   node tools/browser_shots.mjs --dir build/web/samples --out build/shots/browser [--browser chromium|firefox]
//        [--gpu swiftshader|hardware] [--wait 1500] [sample ...]
//
// The pages are served from --dir by a small static server on 127.0.0.1. A sample passes when its page shows a
// canvas, logs no error and no uncaught exception. Exit code: 0 when every sample passes.
import { chromium, firefox } from 'playwright';
import { createServer } from 'node:http';
import { readFile, mkdir, readdir } from 'node:fs/promises';
import { extname, join, resolve } from 'node:path';

const args = process.argv.slice(2);
const opt = (name, fallback) => {
  const i = args.indexOf(name);
  if (i < 0) return fallback;
  const v = args[i + 1];
  args.splice(i, 2);
  return v;
};
const dir = resolve(opt('--dir', 'build/web/samples'));
const out = resolve(opt('--out', 'build/shots/browser'));
const browserName = opt('--browser', 'chromium');
const gpu = opt('--gpu', 'swiftshader');
const wait = Number(opt('--wait', '1500'));
const only = args;

const types = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream' };
const server = createServer(async (req, res) => {
  try {
    const path = join(dir, decodeURIComponent(new URL(req.url, 'http://x').pathname));
    if (!path.startsWith(dir)) throw new Error('outside');
    const body = await readFile(path);
    res.writeHead(200, { 'Content-Type': types[extname(path)] ?? 'application/octet-stream' });
    res.end(body);
  } catch {
    res.writeHead(404);
    res.end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const base = `http://127.0.0.1:${server.address().port}/`;

const samples = (await readdir(dir)).filter((f) => f.endsWith('.html')).map((f) => f.slice(0, -5))
  .filter((s) => !only.length || only.includes(s)).sort();
await mkdir(out, { recursive: true });

const launchArgs = browserName === 'chromium'
  ? (gpu === 'hardware' ? ['--enable-gpu', '--ignore-gpu-blocklist', '--use-angle=default'] : ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'])
  : [];
const browser = await (browserName === 'firefox' ? firefox : chromium).launch({ headless: true, args: launchArgs });
let failed = 0;
for (const name of samples) {
  const page = await browser.newPage({ viewport: { width: 800, height: 600 } });
  const problems = [];
  page.on('console', (m) => { if (m.type() === 'error') problems.push(m.text()); });
  page.on('pageerror', (e) => problems.push(e.message));
  await page.goto(base + name + '.html');
  await page.waitForTimeout(wait);
  // The canvas at whole pixels in the page's top left corner, so the screenshot holds its pixels and nothing else.
  await page.addStyleTag({ content: 'body{display:block!important;margin:0!important} canvas{position:fixed!important;left:0!important;top:0!important}' });
  await page.waitForTimeout(100);
  const size = await page.evaluate(() => { const c = document.getElementById('canvas'); return c ? [c.width, c.height] : null; });
  const file = join(out, `${name}.png`);
  if (size) await page.locator('#canvas').screenshot({ path: file });
  const ok = size && size[0] > 1 && problems.length === 0;
  if (!ok) failed++;
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name} ${size ? size.join('x') : 'no canvas'}${problems.length ? ' — ' + problems.join(' | ') : ''}`);
  await page.close();
}
await browser.close();
server.close();
console.log(`${samples.length - failed}/${samples.length} samples ran; screenshots in ${out}`);
process.exit(failed ? 1 : 0);
