<script>
  import Sidebar from './lib/Sidebar.svelte';
  import EndpointCard from './lib/EndpointCard.svelte';
  import { onMount } from 'svelte';
  import { groups as fallbackGroups, fetchApiInfo, groupsFromApiInfo } from './lib/api.js';
  let filter = '';
  let active = '';
  let baseUrl = localStorage.getItem('pm_base') || 'http://localhost:8080';
  let token = localStorage.getItem('pm_token') || '';
  let groups = fallbackGroups;
  $: endpointCount = groups.reduce((n, g) => n + g.items.length, 0);
  $: methodCount = groups.reduce((n, g) => n + g.items.reduce((x, item) => x + item.methods.length, 0), 0);
  $: visible = groups.map((g) => ({ ...g, items: g.items.filter((x) => !filter || `${x.path} ${x.title} ${g.name}`.toLowerCase().includes(filter.toLowerCase())) })).filter((g) => g.items.length);
  onMount(async () => {
    try {
      groups = groupsFromApiInfo(await fetchApiInfo(baseUrl));
    } catch (error) {
      console.warn('Using fallback API documentation:', error);
    }
  });
  function save() { localStorage.setItem('pm_base', baseUrl); localStorage.setItem('pm_token', token); }
  function select(path) { active = path; document.getElementById(`endpoint-${path.replaceAll('/', '-').replace(/[{}]/g, '')}`)?.scrollIntoView({ behavior: 'smooth', block: 'start' }); }
</script>

<header class="topbar">
  <div class="brand-mark">P</div>
  <div class="brand-copy"><strong>Priemman <em>API</em></strong><small>REST + protobuf · v1</small></div>
  <div class="top-spacer"></div>
  <label><span>Base URL</span><input bind:value={baseUrl} on:input={save} /></label>
  <label class="token-field"><span>Session token</span><input bind:value={token} on:input={save} placeholder="sess_…" /></label>
</header>

<div class="layout">
  <div class="sidebar-shell">
    <div class="sidebar-heading"><span>API reference</span><small>{endpointCount} endpoints</small></div>
    <input class="search" bind:value={filter} placeholder="Search endpoints…" aria-label="Search endpoints" />
    <Sidebar {groups} {filter} {active} onSelect={select} />
  </div>
  <main>
    <section class="hero">
      <div class="eyebrow">Workspace / API collection</div>
      <h1>Priemman Backend API</h1>
      <p>Dokumentasi endpoint dari konfigurasi server dan protobuf. Payload production menggunakan <code>application/x-protobuf</code>.</p>
      <div class="stats"><span><b>{endpointCount}</b> paths</span><span><b>{methodCount}</b> methods</span><span><b>60</b> req/min</span></div>
    </section>
    {#if !visible.length}
      <div class="empty-state"><strong>No endpoints found</strong><span>Coba kata kunci lain untuk pencarian endpoint.</span></div>
    {:else}
      {#each visible as group}
        <section class="group-section">
          <div class="section-title"><h2>{group.name}</h2><span>{group.items.length} endpoint{group.items.length === 1 ? '' : 's'}</span></div>
          {#each group.items as endpoint}<EndpointCard {endpoint} {baseUrl} {token} />{/each}
        </section>
      {/each}
    {/if}
  </main>
</div>
