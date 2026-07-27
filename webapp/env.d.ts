/// <reference types="vite/client" />

declare const __TOST_ENABLED__: boolean;

import { Router, Route } from 'vue-router'
declare module 'vue' {
  interface ComponentCustomProperties {
    $router: Router
    $route: Route
  }
}
