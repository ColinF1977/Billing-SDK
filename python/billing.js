(() => {
  const panel = document.createElement('aside');
  panel.setAttribute('aria-label', 'Shared billing');
  panel.style.cssText = 'padding:12px;margin:12px;border:1px solid #536073;border-radius:6px;display:flex;gap:12px;align-items:center';
  const message = document.createElement('span');
  const button = document.createElement('button'); button.textContent = 'Resume billing';
  panel.append(message, button); document.body.prepend(panel);
  async function refresh() {
    try { const r = await fetch('/api/billing/status'); if(!r.ok)throw new Error('Billing status unavailable'); const s = await r.json(); message.textContent = s.allowed ? 'Using Hyperion shared frame credit' : (s.reason || 'Billing paused'); button.hidden = s.allowed; }
    catch(e) { message.textContent = e.message; }
  }
  button.onclick = async () => { button.disabled=true; try {const r=await fetch('/api/billing/resume',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});const s=await r.json();if(!r.ok)throw new Error(s.error);await refresh();}catch(e){message.textContent=e.message;}finally{button.disabled=false;} };
  refresh();setInterval(refresh,3000);
})();
