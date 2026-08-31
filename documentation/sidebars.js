/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  apiSidebar: [
    'intro',
    'protobuf-api-map',
    {
      type: 'category',
      label: 'Autentikasi',
      items: ['auth'],
    },
    {
      type: 'category',
      label: 'User dan media',
      items: ['users-media'],
    },
    {
      type: 'category',
      label: 'Portfolio',
      items: ['projects', 'collections'],
    },
    {
      type: 'category',
      label: 'Operasional',
      items: ['upgrades-admin', 'reference'],
    },
  ],
};

export default sidebars;
