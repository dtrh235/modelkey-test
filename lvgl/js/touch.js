/**
 * 触屏交互层 — 预览用（实机对应 app_touch_ui）
 */
(function (global) {
  'use strict';

  function initTouch(ctx) {
    const device = document.querySelector('.device-screen');
    if (!device || !ctx) return;

    let sx = 0;
    let sy = 0;
    let st = 0;
    let tracking = false;
    let axis = null;
    let pullY = 0;

    function point(e) {
      if (e.touches && e.touches.length) return { x: e.touches[0].clientX, y: e.touches[0].clientY };
      if (e.changedTouches && e.changedTouches.length) return { x: e.changedTouches[0].clientX, y: e.changedTouches[0].clientY };
      return { x: e.clientX, y: e.clientY };
    }

    function setParallax(x, y) {
      const rect = device.getBoundingClientRect();
      const px = ((x - rect.left) / rect.width - 0.5) * 8;
      const py = ((y - rect.top) / rect.height - 0.5) * 6;
      device.style.setProperty('--px', px.toFixed(2) + 'px');
      device.style.setProperty('--py', py.toFixed(2) + 'px');
    }

    function onStart(e) {
      if (e.target.closest('.modal-overlay.show, .unlock-ripple-layer')) return;
      const p = point(e);
      sx = p.x;
      sy = p.y;
      st = Date.now();
      tracking = true;
      axis = null;
      pullY = 0;
      setParallax(p.x, p.y);
    }

    function onMove(e) {
      if (!tracking) return;
      const p = point(e);
      const dx = p.x - sx;
      const dy = p.y - sy;
      if (!axis) {
        if (Math.abs(dx) > 8 || Math.abs(dy) > 8) {
          axis = Math.abs(dx) > Math.abs(dy) ? 'x' : 'y';
        }
      }
      setParallax(p.x, p.y);

      if (ctx.state.screen === 'wifi' && sy < device.getBoundingClientRect().top + 120 && dy > 0) {
        pullY = Math.min(dy * 0.35, 48);
        device.style.setProperty('--pull', pullY + 'px');
      }
    }

    function onEnd(e) {
      if (!tracking) return;
      tracking = false;
      const p = point(e);
      const dx = p.x - sx;
      const dy = p.y - sy;
      const dt = Date.now() - st;
      device.style.setProperty('--pull', '0px');

      const scr = ctx.state.screen;

      /* 左滑返回（手指向左划） */
      if (axis === 'x' && dx < -56 && Math.abs(dy) < 72 && dt < 520) {
        if (scr !== 'home') ctx.handleBackSwipe();
        return;
      }

      if (scr === 'home' && axis === 'x' && Math.abs(dx) > 36) {
        ctx.state.homeSelected = dx < 0 ? 'unlock' : 'menu';
        ctx.patchHomeOnly();
        return;
      }

      if (axis === 'y') {
        const steps = Math.round(-dy / 42);
        if (steps !== 0) ctx.scrollSteps(steps);
        if (scr === 'wifi' && pullY > 36) ctx.wifiScan();
      }
    }

    device.addEventListener('touchstart', onStart, { passive: true });
    device.addEventListener('touchmove', onMove, { passive: true });
    device.addEventListener('touchend', onEnd);
    device.addEventListener('mousedown', onStart);
    device.addEventListener('mousemove', (e) => { if (e.buttons) onMove(e); });
    device.addEventListener('mouseup', onEnd);
    device.addEventListener('mouseleave', () => { tracking = false; });
  }

  global.LockTouch = { init: initTouch };
})(window);
