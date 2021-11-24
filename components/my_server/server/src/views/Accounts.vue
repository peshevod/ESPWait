<template>
  <v-container fluid class="red lighten-4 d-flex align-start" fill-height>
<!-- Main Card -->
    <v-card class="grey lighten-4 pa-5 flex-grow-1">
      <v-container>
<!-- Toolbar of Main Card -->
        <v-row>    
          <v-toolbar class="lime lighten-3 ma-2">
            <v-container class="d-flex justify-space-between">
              <v-col>
                <v-toolbar-title class="black--text mt-2"><h4>Gateway Accounts</h4></v-toolbar-title>
              </v-col>
              <v-col cols="2">
                <v-btn icon @click="addUser">
                  <v-icon>mdi-account-plus-outline</v-icon>
                </v-btn>
              </v-col> 
            </v-container>
          </v-toolbar>
        </v-row>    
<!-- End of Toolbar of Main Card -->
<!-- Card to Add user -->
<v-row>
            <v-card v-if="overlay_add" class="d-flex justify-center flex-column flex-grow-1 ma-2 mt-5">
              <v-row>
                <v-toolbar class="lime lighten-4 mx-3 mb-5">
                  <v-toolbar-title class="black--text"><h4>Add User</h4></v-toolbar-title>
                </v-toolbar>
              </v-row>
              <v-text-field class="mx-5" counter maxlength="16" :rules="rules" label="Login" v-model="Username"></v-text-field>
              <v-text-field class="mx-5" counter maxlength="16" :rules="rules" label="First Name" v-model="FirstName"></v-text-field>
              <v-text-field class="mx-5" counter maxlength="16" :rules="rules" label="Last Name" v-model="LastName"></v-text-field>
                      <v-text-field
                        class="mx-5"
                        counter
                        maxlength="16"
                        :rules="rules"
                        label="Password"
                        v-model="Password"
                        :append-icon="!show1 ? 'mdi-eye' : 'mdi-eye-off'"
                        @click:append="show1 = !show1"
                        :type="show1 ? 'password' : 'text'">
                      </v-text-field>
                      <v-text-field
                        class="mx-5"
                        counter
                        maxlength="16"
                        :rules="rules"
                        label="Repeat Password"
                        v-model="Password1"
                        :append-icon="!show2 ? 'mdi-eye' : 'mdi-eye-off'"
                        @click:append="show2 = !show2"
                        :type="show2 ? 'password' : 'text'">
                      </v-text-field>
              <v-row class="d-flex justify-center">
                <v-btn @click="add" class="lime lighten-4 black--text ma-5" :disabled="!validate">Add</v-btn>
                <v-btn @click="cancel(0)" class="lime lighten-4 black--text ma-5" :disabled="!validate">Cancel</v-btn>
              </v-row>
            </v-card>
</v-row>
<!-- End of Card to Add user -->
<!-- Cards of users -->
        <v-row>    
          <v-card v-for="(User,index) in result.Users" :key="User.Username" class="ma-2 flex-grow-1">
            <v-container>
<!-- Toolbar of users Cards -->
              <v-row>
                <v-toolbar class="lime lighten-4">
                  <v-toolbar-title class="black--text"><h4>{{User.Username}}</h4></v-toolbar-title>
                </v-toolbar>
              </v-row>
<!-- End of Toolbar of users Cards -->
<!-- User card text -->
              <v-row>
                <v-card-text>{{User.FirstName+' '+User.LastName}}</v-card-text>
              </v-row>
<!-- End of User card text -->
<!-- User card buttons and additions  -->
              <v-container justify="space-between">
<!-- User card buttons -->
                <v-row>
                  <v-col><v-btn @click="edit(index)" class="lime lighten-4 black--text mr-5">Edit</v-btn></v-col>
                  <v-col><v-btn @click="changePassword(index)" class="lime lighten-4 black--text mr-5">Change Password</v-btn></v-col>
                  <v-col><v-btn @click="del(index)" class="lime lighten-4 black--text ml-5">Delete</v-btn></v-col>
                </v-row>
<!-- End of User card buttons -->
<!-- User card addition for Edit -->
                <v-row v-show="overlay_edit[index]" class="justify-center">
                  <v-divider></v-divider>
                  <v-container>
                    <v-card class="d-flex justify-center flex-column pa-5">
                      <v-text-field counter maxlength="16" :rules="rules" label="First Name" v-model="User.FirstName"></v-text-field>
                      <v-text-field counter maxlength="16" :rules="rules" label="Last Name" v-model="User.LastName"></v-text-field>
                      <v-row  class="d-flex justify-center">
                        <v-btn @click="save(index)" class="lime lighten-4 black--text ma-5">Save</v-btn>
                        <v-btn @click="cancel(index)" class="lime lighten-4 black--text ma-5">Cancel</v-btn>
                      </v-row>
                    </v-card>
                  </v-container>
                </v-row>
<!-- End of User card addition for Edit -->
<!-- User card addition for Password -->
                <v-row v-if="overlay_pwd[index]" class="justify-center">
                  <v-divider></v-divider>
                  <v-container>
                    <v-card class="d-flex justify-center flex-column pa-5">
                      <v-text-field
                        counter
                        maxlength="16"
                        :rules="rules"
                        label="Password"
                        v-model="Password"
                        :append-icon="!show1 ? 'mdi-eye' : 'mdi-eye-off'"
                        @click:append="show1 = !show1"
                        :type="show1 ? 'password' : 'text'">
                      </v-text-field>
                      <v-text-field
                        counter
                        maxlength="16"
                        :rules="rules"
                        label="Repeat Password"
                        v-model="Password1"
                        :append-icon="!show2 ? 'mdi-eye' : 'mdi-eye-off'"
                        @click:append="show2 = !show2"
                        :type="show2 ? 'password' : 'text'">
                      </v-text-field>
                      <v-row class="d-flex justify-center">
                        <v-btn @click="change(index)" class="lime lighten-4 black--text ma-5" :disabled="!validate">Change</v-btn>
                        <v-btn @click="cancel(index)" class="lime lighten-4 black--text ma-5" :disabled="!validate">Cancel</v-btn>
                      </v-row>
                    </v-card>
                  </v-container>
                </v-row>
<!-- End of User card addition for Password -->
<!-- User card addition for Delete -->
                <v-row v-show="overlay_del[index]" class="justify-center">
                  <v-divider></v-divider>
                  <v-container>
                    <v-card class="d-flex justify-center flex-column pa-5">
                      <v-row>
                        <v-card-text>Are you sure to delete {{User.Username}}?</v-card-text>
                      </v-row>
                      <v-row  class="d-flex justify-center">
                        <v-btn @click="delUser(index)" class="lime lighten-4 black--text ma-5">Delete</v-btn>
                        <v-btn @click="cancel(index)" class="lime lighten-4 black--text ma-5">Cancel</v-btn>
                      </v-row>
                    </v-card>
                  </v-container>
                </v-row>
<!-- End of User card addition for Delete -->
              </v-container>
<!-- End of User card buttons and additions  -->
            </v-container>
          </v-card>
<!-- End of Cards of users -->
        </v-row>    
      </v-container>
    </v-card>
<!-- End of Main Card -->
  </v-container>
</template>

<script>
// @ is an alias to /src
export default {
  data: () => ({
    result:null,
    Password:null,
    Password1:null,
    Username:null,
    FirstName:null,
    LastName:null,
    overlay_add:false,
    overlay_edit:[],
    overlay_pwd:[],
    overlay_del:[],
    show1: true,
    show2: true,
    rules: [v => v.length <= 16 || 'Max 16 characters'],
  }),
  mounted() {this.get()},
  computed: {
    validate : function() {return this.Password===this.Password1}
  },
  methods : {
    get () {
      this.$ajax.get(this.$store.state.server+'/accounts',{
        headers: {
          'Authorization': 'Bearer ' + this.$store.state.jwt_token
        }
      })
      .then(data => {
        this.result=data.data;
        this.result.Users.forEach((currentValue,index) =>
        {
          this.overlay_edit[index]=false;
          this.overlay_pwd[index]=false;
          this.overlay_del[index]=false;
        });
        this.result.overlay_add=false;  
      })
      .catch(e => {   
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
        }
        if (e.response && e.response.status === 401) {
           this.$router.push({ name: "login" });
        } 
      });
    },
    delUser (index) {
      this.$ajax.post(this.$store.state.server+'/accounts',{
        'Username': this.result.Users[index].Username,
        'Delete': 'true'
      },
      {
          headers: {
            'Authorization': 'Bearer ' + this.$store.state.jwt_token
          }
      })
      .then ()
      .catch(e => {   
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
        }
        if (e.response && e.response.status === 401) {
           this.$router.push({ name: "login" });
        } 
      });
      this.get();
    },
    change (index) {
      this.$ajax.post(this.$store.state.server+'/accounts',{
        'Username': this.result.Users[index].Username,
        'Password': this.Password
      },
      {
          headers: {
            'Authorization': 'Bearer ' + this.$store.state.jwt_token
          }
      })
      .then ()
      .catch(e => {   
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
        }
        if (e.response && e.response.status === 401) {
           this.$router.push({ name: "login" });
        } 
      });
      this.overlay_pwd[index]=false;
      this.$forceUpdate();
    },
    edit (index) {
//      console.log('index='+index+' '+this.overlay_edit[index].toString());
      this.overlay_pwd[index]=false;
      this.overlay_edit[index]=!this.overlay_edit[index];
      this.overlay_del[index]=false;
      this.overlay_add=false;
      this.$forceUpdate();
    },
    changePassword (index) {
      this.overlay_pwd[index]=!this.overlay_pwd[index];
      this.overlay_edit[index]=false;
      this.overlay_del[index]=false;
      this.overlay_add=false;
      this.$forceUpdate();
    },
    del (index) {
      this.overlay_pwd[index]=false;
      this.overlay_edit[index]=false;
      this.overlay_del[index]=!this.overlay_del[index];
      this.overlay_add=false;
      this.$forceUpdate();
    },
    addUser () {
      this.overlay_add=!this.overlay_add;
      this.$forceUpdate();
    },
    cancel (index) {
      this.overlay_pwd[index]=false;
      this.overlay_edit[index]=false;
      this.overlay_del[index]=false;
      this.overlay_add=false;
      this.$forceUpdate();
   },
    add () {
      this.overlay_add=false;
      console.log(this.FirstName.toString());
      this.$ajax.post(this.$store.state.server+'/accounts',{
        'Username': this.Username,
        'FirstName': this.FirstName,
        'LastName': this.LastName,
        'Password': this.Password
      },
      {
          headers: {
            'Authorization': 'Bearer ' + this.$store.state.jwt_token
          }
      })
      .then ()      .catch(e => {   
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
        }
        if (e.response && e.response.status === 401) {
           this.$router.push({ name: "login" });
        } 
      });
      this.get();
    },
    save (index) {
      this.overlay_edit[index]=false;
      this.$ajax.post(this.$store.state.server+'/accounts',{
        'Username': this.result.Users[index].Username,
        'FirstName': this.result.Users[index].FirstName,
        'LastName': this.result.Users[index].LastName
      },
      {
          headers: {
            'Authorization': 'Bearer ' + this.$store.state.jwt_token
          }
      })
      .then ()      .catch(e => {   
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
        }
        if (e.response && e.response.status === 401) {
           this.$router.push({ name: "login" });
        } 
      });
      this.get();
    }
  }
}
</script>
