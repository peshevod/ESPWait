<template>
  <v-app>
    <v-navigation-drawer v-model="drawer" class="grey lighten-4" fixed app clipped>
      <v-list dense>
<!--
	<v-list-subtitle><span class="grey--text text-subtitle-2" v-if="!get_logged_in">not </span></v-list-subtitle>
	<v-list-subtitle><span class="grey--text text-subtitle-2">logged-in</span></v-list-subtitle>
-->
        <v-list-item to="/" v-show="!get_logged_in">
          <v-list-item-action>
            <v-icon>mdi-login-variant</v-icon>
          </v-list-item-action>
          <v-list-item-content>
            <v-list-item-title>Login</v-list-item-title>
          </v-list-item-content>
        </v-list-item>

        <v-list-item to="/settings" v-show="get_logged_in">
          <v-list-item-action>
            <v-icon>mdi-cog-outline</v-icon>
          </v-list-item-action>
          <v-list-item-content>
            <v-list-item-title>Settings</v-list-item-title>
          </v-list-item-content>
        </v-list-item>

        <v-list-item to="/accounts" v-show="get_logged_in">
          <v-list-item-action>
            <v-icon>mdi-account-multiple</v-icon>
          </v-list-item-action>
          <v-list-item-content>
            <v-list-item-title>Accounts</v-list-item-title>
          </v-list-item-content>
        </v-list-item>

        <v-list-item to="/devices" v-show="get_logged_in">
          <v-list-item-action>
            <v-icon>mdi-devices</v-icon>
          </v-list-item-action>
          <v-list-item-content>
            <v-list-item-title>Devices</v-list-item-title>
          </v-list-item-content>
        </v-list-item>
        <v-list-item to="/logout" v-show="get_logged_in">
          <v-list-item-action>
            <v-icon>mdi-logout-variant</v-icon>
          </v-list-item-action>
          <v-list-item-content>
            <v-list-item-title>Logout</v-list-item-title>
          </v-list-item-content>
        </v-list-item>
        <v-list-item to="/about">
          <v-list-item-action>
            <v-icon>mdi-information-variant</v-icon>
          </v-list-item-action>
          <v-list-item-content>
            <v-list-item-title>About</v-list-item-title>
          </v-list-item-content>
        </v-list-item>
      </v-list>
    </v-navigation-drawer>

    <v-app-bar color="lime lighten-2" fixed app clipped-left>
      <v-app-bar-nav-icon @click="drawer = !drawer"></v-app-bar-nav-icon>
<!-- 
     <v-container>
     <v-row>
     <v-col>
-->
     <v-app-bar-title class="text-lg-h6 black--text">ESPWait Administrator</v-app-bar-title>
<!-- 
     <v-spacer></v-spacer>
-->
<!-- 
     </v-col>
     </v-row>
     </v-container>
-->
    </v-app-bar>

    <v-main>
      <v-container fluid fill-height>
        <router-view></router-view>
      </v-container>
    </v-main>

    <v-footer color="lime lighten-2" app fixed>
      <span class="black--text">&copy; ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD. All rights reserved.</span>
    </v-footer>
  </v-app>
</template>

<script>
export default {
  name: "app",
  data() {
    return {
      drawer: null
    };
  },
  computed: {
    get_logged_in() {
      return this.$store.state.logged_in;
    }
  },
  mounted() {
    this.$router.push({ name: "login" });
  }


};
</script>
