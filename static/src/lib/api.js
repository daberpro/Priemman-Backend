const e = (path, title, methods, description = '') => ({ path, title, methods, description });
const m = (method, auth = 'none', extra = {}) => ({ method, auth, ...extra });

export const groups = [
  { name: 'System', items: [e('/', 'Landing Page', [m('GET')]), e('/ping', 'Health Check', [m('GET')]), e('/v1/info', 'Info API', [m('GET')])] },
  { name: 'Auth', items: [
    e('/v1/auth/send-otp', 'Kirim OTP', [m('POST', 'none', { body: { email: 'budi@example.com' } })]),
    e('/v1/auth/verify-otp', 'Verifikasi OTP', [m('POST', 'none', { body: { email: 'budi@example.com', otp: '482913' } })]),
    e('/v1/auth/logout', 'Logout', [m('POST', 'required', { body: { session_token: 'sess_…' } })]),
    e('/v1/auth/google', 'Mulai OAuth Google', [m('GET')]), e('/v1/auth/callback/google', 'Callback OAuth Google', [m('GET')]),
    e('/v1/auth/github', 'Mulai OAuth GitHub', [m('GET')]), e('/v1/auth/callback/github', 'Callback OAuth GitHub', [m('GET')])
  ] },
  { name: 'Projects', items: [
    e('/v1/projects', 'Buat Project', [m('POST', 'required', { body: { input: { title: 'Redesign Landing Page', tags: ['ui-design'] } } })]),
    e('/v1/projects/list', 'Daftar Project', [m('GET', 'required', { query: ['status', 'limit', 'offset'] })]),
    e('/v1/projects/{id}', 'Detail Project', [m('GET', 'optional', { params: ['id'] }), m('PUT', 'required', { params: ['id'], body: { title: 'Redesign v2' } }), m('DELETE', 'required', { params: ['id'] })])
  ] },
  { name: 'Collections', items: [e('/v1/collections', 'CRUD Collection', [m('GET', 'required'), m('POST', 'required', { body: { title: 'UI Inspiration' } }), m('PUT', 'required', { body: { id: 'col_a1', title: 'UI Inspiration 2026' } }), m('DELETE', 'required', { body: { id: 'col_a1' } })])] },
  { name: 'Users', items: [
    e('/v1/users/me', 'Profil Saya', [m('GET', 'required'), m('PUT', 'required', { body: { first_name: 'Budi' } }), m('PATCH', 'required', { body: { headline: 'Design Lead' } })]),
    e('/v1/users/upgrade-logs', 'Riwayat Upgrade', [m('GET', 'required', { query: ['limit', 'offset'] })]),
    e('/v1/users/me/liked', 'Project yang Disukai', [m('GET', 'required', { query: ['limit', 'offset'] }), m('POST', 'required', { body: { project_id: 'prj_7f3k9' } }), m('DELETE', 'required', { body: { project_id: 'prj_7f3k9' } })]),
    e('/v1/users/me/saved', 'Project Tersimpan', [m('GET', 'required', { query: ['limit', 'offset'] }), m('POST', 'required', { body: { project_id: 'prj_7f3k9' } }), m('DELETE', 'required', { body: { project_id: 'prj_7f3k9' } })]),
    e('/v1/users/me/work-experiences', 'Pengalaman Kerja', [m('GET', 'required'), m('POST', 'required', { body: { title: 'UI Designer', company: 'PT Kreatif Digital' } }), m('DELETE', 'required', { body: { id: 'we_1' } })]),
    e('/v1/users/public', 'Profil Publik', [m('GET', 'none', { query: ['user_id'] })]),
    e('/v1/users/public/projects', 'Project Publik User', [m('GET', 'none', { query: ['user_id', 'limit', 'offset'] })])
  ] },
  { name: 'Media', items: [e('/v1/media/upload', 'Upload Media', [m('POST', 'required', { multipart: true })])] },
  { name: 'Workspace', items: [e('/v1/workspace/calendar', 'Kalender Workspace', [m('GET', 'required', { query: ['from', 'to'] })])] },
  { name: 'Upgrade', items: [e('/v1/users/me/upgrade', 'Upgrade Akun', [m('GET', 'required'), m('POST', 'required', { body: { plan: 'creator', payment_proof_media_id: 'med_pay1' } })])] },
  { name: 'Admin', items: [
    e('/v1/admin/users', 'Daftar User', [m('GET', 'admin', { query: ['limit', 'offset', 'role'] })]),
    e('/v1/admin/upgrades', 'Daftar Upgrade', [m('GET', 'admin', { query: ['status'] })]),
    e('/v1/admin/upgrades/review', 'Review Upgrade', [m('POST', 'admin', { body: { id: 'upg_01', approve: true } })]),
    e('/v1/admin/upgrades/confirm-payment', 'Konfirmasi Pembayaran', [m('POST', 'admin', { body: { id: 'upg_01' } })])
  ] }
];

export const endpointCount = groups.reduce((n, g) => n + g.items.length, 0);
export const methodCount = groups.reduce((n, g) => n + g.items.reduce((x, item) => x + item.methods.length, 0), 0);

const groupForPath = (path) => {
  if (path.startsWith('/v1/auth/')) return 'Auth';
  if (path.startsWith('/v1/admin/')) return 'Admin';
  if (path.startsWith('/v1/users/')) return 'Users';
  if (path.startsWith('/v1/projects')) return 'Projects';
  if (path.startsWith('/v1/collections')) return 'Collections';
  if (path.startsWith('/v1/media/')) return 'Media';
  if (path.startsWith('/v1/workspace/')) return 'Workspace';
  return 'System';
};

export function groupsFromApiInfo(info) {
  const grouped = new Map();
  for (const endpoint of info?.endpoints || []) {
    const group = groupForPath(endpoint.path);
    if (!grouped.has(group)) grouped.set(group, []);
    grouped.get(group).push({
      path: endpoint.path,
      title: endpoint.path,
      methods: [m('' + endpoint.method)],
      description: endpoint.description || ''
    });
  }

  return [...grouped].map(([name, items]) => ({ name, items }));
}

export async function fetchApiInfo(baseUrl) {
  const response = await fetch(`${baseUrl.replace(/\/$/, '')}/v1/info`);
  if (!response.ok) throw new Error(`API info request failed: ${response.status}`);
  return response.json();
}

export function curl(endpoint, request, baseUrl, token) {
  let path = endpoint.path.replace('{id}', 'prj_7f3k9');
  if (request.query?.length) path += '?' + request.query.map((key) => `${key}=${key === 'user_id' ? 'usr_abc123' : key === 'status' ? 'pending' : key === 'role' ? 'creator' : key === 'from' ? '2026-09-01' : key === 'to' ? '2026-09-30' : key === 'limit' ? '20' : '0'}`).join('&');
  const lines = [`curl --request ${request.method}`, `  --url '${baseUrl.replace(/\/$/, '')}${path}'`];
  if (request.auth !== 'none') lines.push(`  --header 'Authorization: Bearer ${token || 'sess_…'}'`);
  if (request.multipart) lines.push("  --form 'files=@./example.jpg'");
  else if (!['GET', 'OPTIONS'].includes(request.method)) lines.push("  --header 'Content-Type: application/x-protobuf'", "  --data-binary '@request.pb'");
  return lines.join(' \\\n');
}
