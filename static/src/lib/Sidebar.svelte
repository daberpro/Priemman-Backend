<script>
  export let groups = [];
  export let filter = '';
  export let active = '';
  export let onSelect;
  $: visible = groups.map((g) => ({ ...g, items: g.items.filter((x) => !filter || `${x.path} ${x.title} ${g.name}`.toLowerCase().includes(filter.toLowerCase())) })).filter((g) => g.items.length);
</script>

<aside class="sidebar"><div class="side-heading">Collections</div>{#each visible as group}<div class="group-title">{group.name}</div><nav>{#each group.items as endpoint}<button class="nav-row" class:active={active === endpoint.path} on:click={() => onSelect(endpoint.path)}><span class="method-dots">{#each endpoint.methods as request}<i class="dot {request.method.toLowerCase()}">{request.method}</i>{/each}</span><span class="nav-path">{endpoint.path}</span></button>{/each}</nav>{/each}</aside>
