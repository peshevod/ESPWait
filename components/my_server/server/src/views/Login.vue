<template>
          <v-layout flex align-start justify-left>
            <v-flex xs12 sm4 elevation-6>
              <v-toolbar class="lime lighten-3">
                <v-toolbar-title class="black--text"><h4>Welcome Administrator</h4></v-toolbar-title>
              </v-toolbar>
              <v-card>
                <v-card-text class="pt-4">
                  <div>
                      <v-form>
                        <v-text-field
                          label="Enter your password"
                          v-model="password"
                          class="input-group--focused"
                          :append-icon="!show1 ? 'mdi-eye' : 'mdi-eye-off'"
                          :type="show1 ? 'password' : 'text'"
                          :rules="[rules.required]"
                          counter
                          @click:append="show1 = !show1"
                         ></v-text-field>
                        <v-layout justify-space-between>
                            <v-btn @click="doLogin" class="lime lighten-4 black--text">Login</v-btn>
                        </v-layout>
                      </v-form>
                  </div>
                </v-card-text>
              </v-card>
            </v-flex>
          </v-layout>
</template>

<script>
// @ is an alias to /src
export default {
  data () {
    return {
      result: null,
      valid: false,
      show1: true,
      password: '',
      rules: { required: value => !!value || 'Required.' }
    }
  },
  mounted() {
    this.$store.dispatch('force_logout');
  },
  methods: {
    submit () {
      if (this.$refs.form.validate()) {
        this.$refs.form.$show1.submit()
      }
    },
    clear () {
      this.$refs.form.reset()
    },
    doLogin () {
        console.log("begin");
        const axios = require('axios');
//      var au='Basic ' + btoa('admin' + ':' + this.password);
      axios.get(this.$store.state.server+'/login',{
//        headers: { 'Authorization': au },
        auth: {
          username: 'admin',
          password: this.password
        }
      })
      .then(response => {
        this.result = response.data;
        console.log("response token="+this.result.token);
        if(response.data.token){
          
          axios.get(this.$store.state.server+'/login',{
            headers: {
              'Authorization': 'Bearer ' + this.result.token,
            }
          })
          .then(response => {
            console.log("User="+this.result.user);
            if(this.result.user){
              this.$store.state.logged_in=true;
              this.$store.state.jwt_token=this.result.token;
              this.$router.push({ name: "settings" });
            }else{
              this.error_message=response.data.status;
            }
          })
        }
      })
      .catch(e => {   
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
          this.snackbar=true;
        }
      })
    }
  }
}
</script>
