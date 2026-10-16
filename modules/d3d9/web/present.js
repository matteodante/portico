// --pre-js for PORTICO_D3D9_BITMAP_PRESENT builds: shows the frames the program's worker posts at each D3D9
// Present (src/gl_context_web.cpp). The worker owns #canvas as an OffscreenCanvas, so the page
// cannot draw into it; frames go to a sibling "bitmaprenderer" canvas that takes its place.
if (typeof ENVIRONMENT_IS_PTHREAD === 'undefined' || !ENVIRONMENT_IS_PTHREAD) {
  (function () {
    if (typeof BroadcastChannel === 'undefined' || typeof document === 'undefined') return;
    var screen = null, ctx = null;
    new BroadcastChannel('d3d9gl-present').onmessage = function (e) {
      var bmp = e.data;
      if (!screen) {
        var src = document.getElementById('canvas');
        screen = document.createElement('canvas');
        screen.id = 'd3d9gl-screen';
        screen.className = src ? src.className : '';
        if (src) {
          screen.style.cssText = src.style.cssText;
          src.parentNode.insertBefore(screen, src);
          src.style.display = 'none';
        } else {
          document.body.appendChild(screen);
        }
        ctx = screen.getContext('bitmaprenderer');
      }
      if (screen.width !== bmp.width || screen.height !== bmp.height) {
        screen.width = bmp.width;
        screen.height = bmp.height;
      }
      ctx.transferFromImageBitmap(bmp);
    };
  })();
}
