<script>
  import CodeBlock from './CodeBlock.svelte';
  import { curl } from './api.js';
  export let endpoint;
  export let baseUrl;
  export let token;
  let selected = 0;
  let opened = false;
  $: request = endpoint.methods[selected];
  $: exampleCurl = curl(endpoint, request, baseUrl, token);
  $: body = request.body ? JSON.stringify(request.body, null, 2) : '';
  $: anchor = `endpoint-${endpoint.path.replaceAll('/', '-').replace(/[{}]/g, '')}`;
</script>

<article id={anchor} class="endpoint-card"><div class="endpoint-top"><div class="endpoint-title"><code>{endpoint.path}</code><h3>{endpoint.title}</h3><p>{endpoint.description}</p></div><span class="auth {request.auth}">{request.auth === 'none' ? 'PUBLIC' : request.auth === 'admin' ? 'ADMIN' : 'AUTH'}</span></div><div class="method-tabs">{#each endpoint.methods as method, index}<button class="method {method.method.toLowerCase()}" class:chosen={selected === index} on:click={() => selected = index}>{method.method}</button>{/each}</div><div class="request-line"><span class="method {request.method.toLowerCase()}">{request.method}</span><code>{baseUrl}{endpoint.path}</code><button on:click={() => opened = !opened}>{opened ? 'Hide' : 'Try it'}</button></div>{#if opened}<div class="try-panel">{#if request.params}<div class="hint">Path params: {request.params.join(', ')}</div>{/if}{#if request.query}<div class="hint">Query params: {request.query.join(', ')}</div>{/if}{#if body}<CodeBlock label="Request body · JSON representation" value={body} />{/if}<CodeBlock label="cURL" value={exampleCurl} />{#if request.multipart}<div class="hint">Multipart field: <code>files</code> · maksimum 20 MB</div>{/if}</div>{/if}</article>
