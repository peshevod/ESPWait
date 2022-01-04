import Vue from 'vue'
import Vuex from 'vuex'

Vue.use(Vuex)

export default new Vuex.Store({
  state: {
    logged_in: false,
    jwt_token: null,
    server: 'http://localhost'
  },
  mutations: {
    login() { this.state.logged_in=true; },
    logout() { this.state.logged_in=false; }
  },
  actions: {
    force_login({commit}) { commit("login");},
    force_logout({commit}) { commit("logout");}
  }
})
