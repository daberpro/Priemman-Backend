import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'Priemman API',
  tagline: 'Dokumentasi API backend Priemman',
  favicon: 'img/favicon.svg',
  url: 'https://priemman.my.id',
  baseUrl: '/',
  organizationName: 'daberpro',
  projectName: 'Priemman-Backend',
  trailingSlash: false,
  onBrokenLinks: 'throw',
  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },
  deploymentBranch: 'main',
  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.js',
          routeBasePath: '/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      },
    ],
  ],
  themeConfig: {
    navbar: {
      title: 'Priemman API',
      items: [
        {type: 'docSidebar', sidebarId: 'apiSidebar', position: 'left', label: 'Dokumentasi'},
        {href: 'https://github.com/daberpro/Priemman-Backend', label: 'GitHub', position: 'right'},
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'API',
          items: [{label: 'Mulai', to: '/'}],
        },
        {
          title: 'Project',
          items: [{label: 'GitHub', href: 'https://github.com/daberpro/Priemman-Backend'}],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} Priemman.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  },
};

export default config;
