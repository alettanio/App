// AutoSerwis Pro — aplikacja przeglądarkowa (HTML/CSS/JS)
// Łączy się z API serwera C++ (ten sam origin). Tworzenie/edycja/usuwanie
// serwer dopuszcza tylko z localhost (komputer-serwer) — z telefonu zwróci 403.
'use strict';

const API = ''; // ten sam adres co strona, np. http://192.168.1.10:8080
let orders = [];
let currentDetailId = null;

const $ = (id) => document.getElementById(id);
const els = {
  list: $('ordersList'), empty: $('emptyState'), count: $('countLabel'),
  search: $('search'), status: $('statusFilter'), sort: $('sortSel'),
  statTotal: $('statTotal'), statActive: $('statActive'), statRevenue: $('statRevenue'),
  badge: $('connBadge'), toast: $('toast'),
};

function fmtPLN(v) {
  return new Intl.NumberFormat('pl-PL', { style: 'currency', currency: 'PLN' }).format(Number(v || 0));
}
function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
}
function fmtDate(s) {
  if (!s) return '—';
  const d = new Date(s.length <= 10 ? s + 'T00:00:00' : s);
  return Number.isNaN(d.getTime()) ? esc(s) : d.toLocaleDateString('pl-PL');
}
let toastTimer = null;
function toast(msg, type = 'ok') {
  els.toast.textContent = msg;
  els.toast.className = 'toast ' + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => els.toast.classList.add('hidden'), 4200);
}
function totals(o) {
  const repairs = o.repairs || [];
  const net = repairs.reduce((s, r) => s + Number(r.qty || 0) * Number(r.priceNet || 0), 0);
  const vat = repairs.reduce((s, r) => s + Number(r.qty || 0) * Number(r.priceNet || 0) * Number(r.vatRate || 0) / 100, 0);
  const gross = net + vat;
  const discount = gross * Number(o.discountPct || 0) / 100;
  const toPay = Math.max(0, gross - discount - Number(o.advance || 0));
  return { net, vat, gross, discount, toPay };
}
function pillClass(status) {
  const map = {
    'Przyjęte': 'pill-przyjete', 'W realizacji': 'pill-wrealizacji',
    'Oczekuje na części': 'pill-czesci', 'Gotowe': 'pill-gotowe',
    'Odebrane': 'pill-odebrane', 'Anulowane': 'pill-anulowane',
  };
  return map[status] || 'pill-default';
}

async function api(path, options = {}) {
  const res = await fetch(API + path, { headers: { 'Content-Type': 'application/json' }, ...options });
  if (res.status === 204) return null;
  const text = await res.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch { data = text; }
  if (!res.ok) throw new Error((data && data.error) || ('Błąd serwera HTTP ' + res.status));
  return data;
}

async function checkHealth() {
  try {
    await api('/api/health');
    els.badge.textContent = 'Serwer: połączono';
    els.badge.className = 'badge badge-ok';
    return true;
  } catch {
    els.badge.textContent = 'Serwer: brak połączenia';
    els.badge.className = 'badge badge-bad';
    return false;
  }
}

async function loadStats() {
  try {
    const s = await api('/api/stats');
    els.statTotal.textContent = s.total ?? 0;
    els.statActive.textContent = s.active ?? 0;
    els.statRevenue.textContent = fmtPLN(s.revenue || 0);
  } catch { /* statystyki są pomocnicze */ }
}

async function loadOrders() {
  try {
    orders = await api('/api/orders');
    render();
    await loadStats();
  } catch (e) {
    await checkHealth();
    toast('Nie udało się pobrać zleceń: ' + e.message, 'error');
  }
}

function filteredOrders() {
  const q = els.search.value.trim().toLowerCase();
  const st = els.status.value;
  let out = orders.filter((o) => {
    if (st && o.status !== st) return false;
    if (!q) return true;
    const hay = [o.id, o.clientName, o.clientPhone, o.carBrand, o.carModel, o.carPlate, o.carVin, o.mechanic].join(' ').toLowerCase();
    return hay.includes(q);
  });
  const by = els.sort.value;
  out.sort((a, b) => {
    if (by === 'dateAsc') return String(a.dateReceived).localeCompare(String(b.dateReceived));
    if (by === 'dueAsc') return String(a.dateDue).localeCompare(String(b.dateDue));
    if (by === 'amountDesc') return totals(b).toPay - totals(a).toPay;
    return String(b.dateReceived).localeCompare(String(a.dateReceived));
  });
  return out;
}

function render() {
  const out = filteredOrders();
  els.count.textContent = out.length + ' / ' + orders.length + ' zleceń';
  els.empty.classList.toggle('hidden', out.length !== 0);
  els.list.innerHTML = out.map((o) => {
    const t = totals(o);
    return `
    <article class="order-card">
      <div class="card-top">
        <div>
          <div class="order-id">${esc(o.id)}</div>
          <div class="order-car">${esc(o.carBrand)} ${esc(o.carModel)} ${esc(o.carYear || '')}</div>
        </div>
        <span class="pill ${pillClass(o.status)}">${esc(o.status)}</span>
      </div>
      <div class="order-meta">
        👤 ${esc(o.clientName || '—')}${o.clientPhone ? ' · ' + esc(o.clientPhone) : ''}<br>
        🚗 ${esc(o.carPlate || 'brak tablicy')}${o.carMileage ? ' · ' + esc(o.carMileage) : ''}<br>
        📅 Przyjęto: ${fmtDate(o.dateReceived)} · Termin: ${fmtDate(o.dateDue)}
        ${o.priority && o.priority !== 'Normalny' ? `<br><span class="prio">Priorytet: ${esc(o.priority)}</span>` : ''}
      </div>
      <div class="card-money"><span>Do zapłaty</span><b>${fmtPLN(t.toPay)}</b></div>
      <div class="card-actions">
        <button class="btn btn-ghost btn-sm" onclick="openDetail('${esc(o.id)}')">Podgląd</button>
        <button class="btn btn-ghost btn-sm" onclick="downloadPdf('${esc(o.id)}')">⬇ PDF</button>
        <button class="btn btn-ghost btn-sm" onclick="openEditorById('${esc(o.id)}')">Edytuj</button>
        <button class="btn btn-danger btn-sm" onclick="deleteOrder('${esc(o.id)}')">Usuń</button>
      </div>
    </article>`;
  }).join('');
}

// ---------- Szczegóły ----------
async function openDetail(id) {
  try {
    const o = await api('/api/orders/' + encodeURIComponent(id));
    currentDetailId = o.id;
    const t = totals(o);
    $('detailTitle').textContent = 'Zlecenie ' + o.id;
    const repairs = (o.repairs || []).map((r, i) => `
      <tr><td>${i + 1}</td><td><b>${esc(r.name)}</b><br><small>${esc(r.code || '')}</small></td>
      <td class="num">${esc(r.qty)}</td><td class="num">${fmtPLN(r.priceNet)}</td>
      <td class="num">${esc(r.vatRate)}%</td><td class="num">${fmtPLN(Number(r.qty || 0) * Number(r.priceNet || 0) * (1 + Number(r.vatRate || 0) / 100))}</td></tr>`).join('');
    $('detailBody').innerHTML = `
      <div style="margin-bottom:12px"><span class="pill ${pillClass(o.status)}">${esc(o.status)}</span>
      ${o.priority ? `<span class="prio">Priorytet: ${esc(o.priority)}</span>` : ''}</div>
      <div class="detail-grid">
        <section class="detail-section"><h3>Klient</h3><dl class="kv">
          <dt>Nazwa</dt><dd>${esc(o.clientName || '—')}</dd><dt>Telefon</dt><dd>${esc(o.clientPhone || '—')}</dd>
          <dt>E-mail</dt><dd>${esc(o.clientEmail || '—')}</dd><dt>Adres</dt><dd>${esc(o.clientAddress || '—')}</dd></dl></section>
        <section class="detail-section"><h3>Pojazd</h3><dl class="kv">
          <dt>Auto</dt><dd>${esc(o.carBrand)} ${esc(o.carModel)} ${esc(o.carYear || '')}</dd>
          <dt>Silnik</dt><dd>${esc(o.carEngine || '—')} · ${esc(o.carFuel || '')}</dd>
          <dt>Rejestracja</dt><dd>${esc(o.carPlate || '—')}</dd><dt>VIN</dt><dd>${esc(o.carVin || '—')}</dd>
          <dt>Przebieg</dt><dd>${esc(o.carMileage || '—')}</dd></dl></section>
        <section class="detail-section"><h3>Terminy i serwis</h3><dl class="kv">
          <dt>Przyjęcie</dt><dd>${fmtDate(o.dateReceived)}</dd><dt>Planowane oddanie</dt><dd>${fmtDate(o.dateDue)}</dd>
          <dt>Mechanik</dt><dd>${esc(o.mechanic || '—')}</dd><dt>Gwarancja</dt><dd>${esc(o.warranty || '—')}</dd></dl></section>
        <section class="detail-section"><h3>Opis</h3><dl class="kv">
          <dt>Zgłoszenie</dt><dd>${esc(o.fault || '—')}</dd><dt>Diagnoza</dt><dd>${esc(o.diagnosis || '—')}</dd>
          <dt>Uwagi</dt><dd>${esc(o.notes || '—')}</dd></dl></section>
      </div>
      <section class="detail-section" style="margin-top:14px"><h3>Naprawy i części</h3>
        <div style="overflow:auto"><table class="detail-table"><thead><tr><th>#</th><th>Nazwa</th><th class="num">Ilość</th><th class="num">Netto</th><th class="num">VAT</th><th class="num">Brutto</th></tr></thead>
        <tbody>${repairs || '<tr><td colspan="6">Brak pozycji</td></tr>'}</tbody></table></div>
      </section>
      <div class="totals-box">
        <div><div class="t-label">Netto</div><div class="t-value">${fmtPLN(t.net)}</div></div>
        <div><div class="t-label">VAT</div><div class="t-value">${fmtPLN(t.vat)}</div></div>
        <div><div class="t-label">Brutto</div><div class="t-value">${fmtPLN(t.gross)}</div></div>
        <div><div class="t-label">Rabat / zaliczka</div><div class="t-value">-${fmtPLN(t.discount)} / ${fmtPLN(o.advance || 0)}</div></div>
        <div class="t-pay"><div class="t-label">Do zapłaty</div><div class="t-value">${fmtPLN(t.toPay)}</div></div>
      </div>`;
    $('detailModal').classList.remove('hidden');
  } catch (e) { toast(e.message, 'error'); }
}
function closeDetail() { $('detailModal').classList.add('hidden'); currentDetailId = null; }

function downloadPdf(id) {
  const a = document.createElement('a');
  a.href = API + '/api/orders/' + encodeURIComponent(id) + '/pdf';
  a.download = 'zlecenie-' + id + '.pdf';
  document.body.appendChild(a); a.click(); a.remove();
}

// ---------- Edytor ----------
function today() { return new Date().toISOString().slice(0, 10); }
function setVal(id, v) { $(id).value = v ?? ''; }
function getVal(id) { return $(id).value.trim(); }
function getNum(id) { const v = parseFloat($(id).value.replace(',', '.')); return Number.isFinite(v) ? v : 0; }

function addRepairRow(r = {}) {
  const tr = document.createElement('tr');
  tr.innerHTML = `
    <td><input class="r-name" placeholder="np. Wymiana klocków hamulcowych" value="${esc(r.name || '')}"></td>
    <td><input class="r-code" placeholder="KOD" value="${esc(r.code || '')}"></td>
    <td><input class="r-qty" type="number" min="0" step="0.1" value="${r.qty ?? 1}"></td>
    <td><input class="r-price" type="number" min="0" step="0.01" value="${r.priceNet ?? 0}"></td>
    <td><input class="r-vat" type="number" min="0" step="1" value="${r.vatRate ?? 23}"></td>
    <td class="r-gross">0,00 zł</td>
    <td><button type="button" class="icon-btn" title="Usuń pozycję">✕</button></td>`;
  tr.querySelector('button').addEventListener('click', () => { tr.remove(); recalcEditor(); });
  tr.querySelectorAll('input').forEach((inp) => inp.addEventListener('input', recalcEditor));
  $('repairsBody').appendChild(tr);
  recalcEditor();
}
function readRepairs() {
  return [...$('repairsBody').querySelectorAll('tr')].map((tr) => ({
    name: tr.querySelector('.r-name').value.trim(),
    code: tr.querySelector('.r-code').value.trim(),
    qty: parseFloat(tr.querySelector('.r-qty').value) || 0,
    priceNet: parseFloat(tr.querySelector('.r-price').value) || 0,
    vatRate: parseFloat(tr.querySelector('.r-vat').value) || 0,
  })).filter((r) => r.name || r.priceNet);
}
function editorOrder() {
  return {
    id: getVal('f_id'), clientName: getVal('f_clientName'), clientPhone: getVal('f_clientPhone'),
    clientEmail: getVal('f_clientEmail'), clientAddress: getVal('f_clientAddress'),
    carBrand: getVal('f_carBrand'), carModel: getVal('f_carModel'), carYear: getVal('f_carYear'),
    carEngine: getVal('f_carEngine'), carPlate: getVal('f_carPlate'), carVin: getVal('f_carVin'),
    carMileage: getVal('f_carMileage'), carFuel: getVal('f_carFuel'), dateReceived: getVal('f_dateReceived'),
    dateDue: getVal('f_dateDue'), status: getVal('f_status'), priority: getVal('f_priority'),
    mechanic: getVal('f_mechanic'), warranty: getVal('f_warranty'), fault: getVal('f_fault'),
    diagnosis: getVal('f_diagnosis'), notes: getVal('f_notes'), discountPct: getNum('f_discountPct'),
    advance: getNum('f_advance'), repairs: readRepairs(),
  };
}
function recalcEditor() {
  document.querySelectorAll('#repairsBody tr').forEach((tr) => {
    const qty = parseFloat(tr.querySelector('.r-qty').value) || 0;
    const price = parseFloat(tr.querySelector('.r-price').value) || 0;
    const vat = parseFloat(tr.querySelector('.r-vat').value) || 0;
    tr.querySelector('.r-gross').textContent = fmtPLN(qty * price * (1 + vat / 100));
  });
  const t = totals(editorOrder());
  $('totalsBox').innerHTML = `
    <div><div class="t-label">Netto</div><div class="t-value">${fmtPLN(t.net)}</div></div>
    <div><div class="t-label">VAT</div><div class="t-value">${fmtPLN(t.vat)}</div></div>
    <div><div class="t-label">Brutto</div><div class="t-value">${fmtPLN(t.gross)}</div></div>
    <div><div class="t-label">Rabat</div><div class="t-value">-${fmtPLN(t.discount)}</div></div>
    <div class="t-pay"><div class="t-label">Do zapłaty</div><div class="t-value">${fmtPLN(t.toPay)}</div></div>`;
}
function fillEditor(o = {}) {
  setVal('f_id', o.id || ''); setVal('f_clientName', o.clientName || ''); setVal('f_clientPhone', o.clientPhone || '');
  setVal('f_clientEmail', o.clientEmail || ''); setVal('f_clientAddress', o.clientAddress || '');
  setVal('f_carBrand', o.carBrand || ''); setVal('f_carModel', o.carModel || ''); setVal('f_carYear', o.carYear || '');
  setVal('f_carEngine', o.carEngine || ''); setVal('f_carPlate', o.carPlate || ''); setVal('f_carVin', o.carVin || '');
  setVal('f_carMileage', o.carMileage || ''); setVal('f_carFuel', o.carFuel || 'Benzyna');
  setVal('f_dateReceived', o.dateReceived || today()); setVal('f_dateDue', o.dateDue || today());
  setVal('f_status', o.status || 'Przyjęte'); setVal('f_priority', o.priority || 'Normalny');
  setVal('f_mechanic', o.mechanic || ''); setVal('f_warranty', o.warranty || '12 miesięcy na części i robociznę');
  setVal('f_fault', o.fault || ''); setVal('f_diagnosis', o.diagnosis || ''); setVal('f_notes', o.notes || '');
  setVal('f_discountPct', o.discountPct ?? 0); setVal('f_advance', o.advance ?? 0);
  $('repairsBody').innerHTML = '';
  (o.repairs && o.repairs.length ? o.repairs : [{}]).forEach(addRepairRow);
  recalcEditor();
}
function openEditor(o = null) {
  $('editorTitle').textContent = o && o.id ? ('Edycja zlecenia ' + o.id) : 'Nowe zlecenie';
  fillEditor(o || {});
  $('editorModal').classList.remove('hidden');
}
async function openEditorById(id) {
  try { openEditor(await api('/api/orders/' + encodeURIComponent(id))); }
  catch (e) { toast(e.message, 'error'); }
}
function closeEditor() { $('editorModal').classList.add('hidden'); }

async function saveEditor(ev) {
  ev.preventDefault();
  const o = editorOrder();
  try {
    if (o.id) await api('/api/orders/' + encodeURIComponent(o.id), { method: 'PUT', body: JSON.stringify(o) });
    else await api('/api/orders', { method: 'POST', body: JSON.stringify(o) });
    closeEditor();
    toast(o.id ? 'Zapisano zmiany.' : 'Utworzono nowe zlecenie.');
    await loadOrders();
  } catch (e) {
    toast(e.message + ' Pamiętaj: tworzenie i edycja działają tylko na komputerze-serwerze.', 'error');
  }
}
async function deleteOrder(id) {
  if (!confirm('Usunąć zlecenie ' + id + '? Tej operacji nie można cofnąć.')) return;
  try {
    await api('/api/orders/' + encodeURIComponent(id), { method: 'DELETE' });
    toast('Usunięto zlecenie ' + id);
    await loadOrders();
  } catch (e) { toast(e.message, 'error'); }
}

// ---------- Zdarzenia ----------
$('btnNew').addEventListener('click', () => openEditor());
$('editorForm').addEventListener('submit', saveEditor);
['f_discountPct', 'f_advance'].forEach((id) => $(id).addEventListener('input', recalcEditor));
[els.search, els.status, els.sort].forEach((el) => { el.addEventListener('input', render); el.addEventListener('change', render); });
$('btnPdf').addEventListener('click', () => { if (currentDetailId) downloadPdf(currentDetailId); });
$('btnPrint').addEventListener('click', () => { if (currentDetailId) window.open(API + '/api/orders/' + encodeURIComponent(currentDetailId) + '/pdf', '_blank'); });
document.querySelectorAll('.modal').forEach((m) => m.addEventListener('click', (e) => { if (e.target === m) m.classList.add('hidden'); }));
document.addEventListener('keydown', (e) => { if (e.key === 'Escape') document.querySelectorAll('.modal').forEach((m) => m.classList.add('hidden')); });

// Funkcje wywoływane z HTML (onclick)
window.openEditor = openEditor; window.closeEditor = closeEditor; window.closeDetail = closeDetail;
window.openDetail = openDetail; window.openEditorById = openEditorById; window.downloadPdf = downloadPdf;
window.deleteOrder = deleteOrder; window.addRepairRow = addRepairRow;

checkHealth();
loadOrders();
setInterval(checkHealth, 30000);
