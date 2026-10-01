<script>
  export let label = '';
  export let value = '';
  let copied = false;
  async function copy() { await navigator.clipboard?.writeText(value); copied = true; setTimeout(() => copied = false, 900); }
</script>
<div class="code-wrap"><div class="code-label">{label}</div><pre>{value}</pre><button class="copy" on:click={copy}>{copied ? 'COPIED' : 'COPY'}</button></div>
