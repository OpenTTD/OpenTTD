// Not a test: imported first by the tests of page scripts. In the browser static/shell.js is the
// first script of every page and publishes its Pro class sets as globalThis.PanelUI before any page
// script runs; here they are published the same way, from the same file, so a page script reads the
// one set (static/components.js UI) and never a copy.
import { createRequire } from 'node:module';

export const { UI } = createRequire(import.meta.url)('../static/shell.js');
globalThis.PanelUI = UI;
