import Vue from 'vue'
import VueRouter from 'vue-router'
import Login from './views/Login.vue'
import Logout from './views/Logout.vue'
import About from './views/About.vue'
import Devices from './views/Devices.vue'
import Accounts from './views/Accounts.vue'
import Settings from './views/Settings.vue'


Vue.use(VueRouter)

const routes = [
  {
    path: '/',
    name: 'login',
    component: Login
  },
  {
    path: '/logout',
    name: 'logout',
    component: Logout
  },
  {
    path: '/about',
    name: 'about',
    component: About
  },
  {
    path: '/devices',
    name: 'devices',
    component: Devices
  }, 
  {
    path: '/accounts',
    name: 'accounts',
    component: Accounts
  },
  {
    path: '/settings',
    name: 'settings',
    component: Settings
  }
]

const router = new VueRouter({
  mode: 'history',
  base: process.env.BASE_URL,
  routes
})

export default router
