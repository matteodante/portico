// Copyright (c) 2026 Matteo Dante
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was
// not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Verify an already published demo. Unlike browser_shots.mjs, this does not test a local build's parity.
// Usage: node tools/check_demo.mjs [https://host/path/index.html]
import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { chromium } from 'playwright';

const index = new URL(process.argv[2] ?? 'https://matteodante.it/portico-demo/index.html');
const samples = ['tut01_createdevice', 'tut02_vertices', 'tut03_matrices', 'tut04_lights', 'tut05_textures'];
const out = 'build/shots/live-demo';
await mkdir(out, { recursive: true });
const proxy = process.env.HTTPS_PROXY ?? process.env.HTTP_PROXY;
const browser = await chromium.launch({
  headless: true,
  args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
  ...(proxy ? { proxy: { server: proxy, bypass: 'localhost,127.0.0.1' } } : {}),
});
let failed = 0;
try {
  const context = await browser.newContext({ viewport: { width: 800, height: 600 } });
  context.setDefaultTimeout(30_000);
  const landing = await context.newPage();
  const response = await landing.goto(index.href);
  assert(response?.ok(), `Demo index returned HTTP ${response?.status()}`);
  assert.match(await landing.title(), /Portico/);
  const links = await landing.locator('a').evaluateAll((nodes) => nodes.map((a) => a.href));
  for (const sample of samples) {
    assert(links.includes(new URL(`${sample}.html`, index).href), `Index has no link to ${sample}`);
  }
  await landing.screenshot({ path: `${out}/index.png`, fullPage: true });
  await landing.close();

  // Check the actual text too: some hosts return the site's HTML fallback with HTTP 200 for missing files.
  for (const [file, required] of [
    ['LICENSE.txt', ['Mozilla Public License Version 2.0']],
    ['LICENSE-samples.txt', ['Permission is hereby granted', 'Microsoft']],
    ['THIRD-PARTY-LICENSES.txt', ['EMSCRIPTEN.txt', 'musl.txt', 'musl-math.txt', 'zlib.txt']],
  ]) {
    assert(links.includes(new URL(file, index).href), `Index has no link to ${file}`);
    const notice = await context.request.get(new URL(file, index).href);
    assert(notice.ok(), `${file} returned HTTP ${notice.status()}`);
    const text = await notice.text();
    assert(!/^\s*<!doctype html/i.test(text), `${file} returned an HTML fallback`);
    for (const phrase of required) assert(text.includes(phrase), `${file} is missing ${phrase}`);
    console.log(`ok   ${file}`);
  }

  for (const [i, name] of samples.entries()) {
    const page = await context.newPage();
    const problems = [];
    const assets = new Set();
    page.on('pageerror', (error) => problems.push(error.message));
    page.on('console', (message) => {
      if (message.type() === 'error') problems.push(message.text());
    });
    page.on('requestfailed', (request) => problems.push(`${request.url()}: ${request.failure()?.errorText}`));
    page.on('response', (response) => {
      const path = new URL(response.url()).pathname;
      if (response.status() >= 400) problems.push(`HTTP ${response.status()}: ${path}`);
      if (response.ok()) assets.add(path);
      if (path.endsWith('.wasm') && !response.headers()['content-type']?.startsWith('application/wasm')) {
        problems.push(`Wrong WebAssembly content type: ${path}`);
      }
    });
    // A canvas alone exists even when Wasm fails to start. Observe real WebGL calls to prove rendering starts.
    await page.addInitScript(() => {
      globalThis.porticoDemoCheck = { clears: 0, draws: 0 };
      for (const [method, counter] of [['clear', 'clears'], ['drawArrays', 'draws'], ['drawElements', 'draws']]) {
        const original = WebGL2RenderingContext.prototype[method];
        WebGL2RenderingContext.prototype[method] = function (...args) {
          const result = original.apply(this, args);
          globalThis.porticoDemoCheck[counter]++;
          return result;
        };
      }
    });
    try {
      const response = await page.goto(new URL(`${name}.html`, index).href);
      assert(response?.ok(), `HTTP ${response?.status()}`);
      await page.waitForFunction((needsDraws) => {
        const counts = globalThis.porticoDemoCheck;
        // Tutorial 1 paints once on WM_PAINT; tutorials 2–5 render continuously.
        return needsDraws ? counts?.clears >= 2 && counts.draws >= 2 : counts?.clears >= 1;
      }, i > 0);
      const size = await page.locator('#canvas').evaluate((canvas) => [canvas.width, canvas.height]);
      assert(size[0] > 1 && size[1] > 1, `Invalid canvas size: ${size}`);
      assert.equal(await page.locator('#message').innerText(), '', 'Sample displays an error message');
      for (const extension of ['js', 'wasm', ...(i === 4 ? ['data'] : [])]) {
        const path = new URL(`${name}.${extension}`, index).pathname;
        assert(assets.has(path), `Sample did not load ${path}`);
      }
      await page.locator('#canvas').screenshot({ path: `${out}/${name}.png` });
      assert.deepEqual(problems, [], 'Browser errors');
      console.log(`ok   ${name}: ${size.join('x')}, rendering, assets loaded, no browser errors`);
    } catch (error) {
      failed++;
      console.error(`FAIL ${name}: ${error.message}${problems.length ? ` — ${problems.join(' | ')}` : ''}`);
      await page.screenshot({ path: `${out}/${name}-failed.png` }).catch(() => {});
    } finally {
      await page.close();
    }
  }
  console.log(`${samples.length - failed}/${samples.length} published samples passed; screenshots in ${out}`);
} finally {
  await browser.close();
}
process.exitCode = failed ? 1 : 0;
