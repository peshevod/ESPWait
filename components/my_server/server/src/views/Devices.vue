<template>
  <v-container fluid class="red lighten-4 d-flex align-start justify-space-between" fill-height>
<!-- Main Card -->
    <v-card class="grey lighten-4 pa-5">
      <v-container>
<!-- Toolbar of Main Card -->
        <v-row>    
          <v-toolbar class="lime lighten-3 ma-2">
            <v-container class="d-flex">
              <v-col>
                <v-toolbar-title class="black--text mt-2"><h4>Devices</h4></v-toolbar-title>
              </v-col>
              <v-col cols="auto">
                <v-btn icon @click="addDevice">
                  <v-icon>mdi-shield-plus-outline</v-icon>
                </v-btn>
              </v-col> 
            </v-container>
          </v-toolbar>
        </v-row>    
<!-- End of Toolbar of Main Card -->
<!-- Card to Add device -->
<v-row>
            <v-card v-if="overlay_add" class="d-flex justify-center flex-column ma-2 mt-3 flex-grow-1">
              <v-row>
                <v-toolbar class="lime lighten-4 ma-3 mb-7">
                  <v-toolbar-title class="black--text"><h4>Add Device</h4></v-toolbar-title>
                </v-toolbar>
              </v-row>
              <v-text-field outlined counter maxlength="16" :rules="rules" label="Name" v-model="DevName" class="mx-5"></v-text-field>
              <v-text-field outlined counter maxlength="16" :rules="rules" label="EUI" v-model="DevEUI" class="mx-5"></v-text-field>
              <v-select outlined counter maxlength="32" :rules="rules" label="LoRa Version" :items="Versions" v-model="Version" class="mx-5"></v-select>
              <v-text-field
                class="mx-5"
                outlined
                counter
                maxlength="32"
                :rules="rules"
                label="Application Key"
                v-model="AppKey"
                :append-icon="!show1 ? 'mdi-eye' : 'mdi-eye-off'"
                @click:append="show1 = !show1"
                :type="show1 ? 'password' : 'text'"></v-text-field>
              <v-text-field
                v-if="Version==='1.1'"
                class="mx-5"
                outlined
                counter
                maxlength="32"
                :rules="rules"
                label="Network Key"
                v-model="NwkKey"
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
          <v-card v-for="(Device,index) in result.Devices" :key="Device.DevEUI" class="ma-2 flex-grow-1">
            <v-container>
<!-- Toolbar of users Cards -->
              <v-row>
                <v-toolbar class="lime lighten-4">
                  <v-toolbar-title class="black--text"><h4>{{Device.DevName}}</h4></v-toolbar-title>
                </v-toolbar>
              </v-row>
<!-- End of Toolbar of users Cards -->
<!-- User card text -->
              <v-row justify-space-between>

<v-simple-table class="mt-3">
<template v-slot:default>
<tbody>
  <tr>
    <td><v-chip label class="grey lighten-4">EUI</v-chip></td>
    <td><v-chip label class="blue lighten-5">{{Device.DevEUI}}</v-chip></td>
  </tr>
  <tr>
    <td><v-chip label class="grey lighten-4">Application Key</v-chip></td>
    <td>
      <v-chip @mousedown="click_app_down(index)" @mouseup="click_app_up(index)" label class="myFont blue lighten-5">
        {{view_app[index] ? Device.AppKey :
'\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF'
        }}
        <v-icon right>mdi-eye</v-icon>
      </v-chip>
    </td>
  </tr>
  <tr v-if="Device.Version=='1.1'">
    <td><v-chip label class="grey lighten-4">Network Key</v-chip></td>
    <td>
      <v-chip @mousedown="click_nwk_down(index)" @mouseup="click_nwk_up(index)" label class="myFont blue lighten-5">
        {{view_nwk[index] ? Device.NwkKey :
 '\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF\u25CF'
        }}
        <v-icon right>mdi-eye</v-icon>
     </v-chip></td>
  </tr>
  <tr>
    <td><v-chip label class="grey lighten-4">LoRa Version</v-chip></td>
    <td><v-chip label class="blue lighten-5">{{Device.Version}}</v-chip></td>
  </tr>

</tbody>
</template>
</v-simple-table>
              </v-row>
<!-- End of User card text -->
<!-- User card buttons and additions  -->
              <v-container justify="space-between">
<!-- User card buttons -->
                <v-row>
                  <v-col><v-btn @click="edit(index)" class="lime lighten-4 black--text mt-3">Edit</v-btn></v-col>
                  <v-col cols="auto"><v-btn @click="del(index)" class="lime lighten-4 black--text mt-3">Delete</v-btn></v-col>
                </v-row>
<!-- End of User card buttons -->
<!-- User card addition for Edit -->
                <v-row v-show="overlay_edit[index]" class="justify-center">
                  <v-divider></v-divider>
                  <v-container>
                    <v-card class="d-flex justify-center flex-column pa-5">
                      <v-text-field outlined counter maxlength="16" :rules="rules" label="Device Name" v-model="result.Devices[index].DevName"></v-text-field>
                      <v-text-field autofocus outlined counter maxlength="16" :rules="rules16" label="EUI" v-model="result.Devices[index].DevEUI"></v-text-field>
                      <v-text-field
                        autofocus 
                        outlined
                        counter
                        maxlength="32"
                        :rules="rules32"
                        label="Application Key"
                        v-model="result.Devices[index].AppKey"
                        :append-icon="!show1 ? 'mdi-eye' : 'mdi-eye-off'"
                        @click:append="show1 = !show1"
                        :type="show1 ? 'password' : 'text'">
                      </v-text-field>
                      <v-text-field
                        v-if="Device.Version==='1.1'"
                        autofocus  
                        outlined
                        counter
                        maxlength="32"
                        :rules="rules32"
                        label="Network Key"
                        v-model="result.Devices[index].NwkKey"
                        :append-icon="!show2 ? 'mdi-eye' : 'mdi-eye-off'"
                        @click:append="show2 = !show2"
                        :type="show2 ? 'password' : 'text'">                       </v-text-field>
                      <v-select outlined counter maxlength="32" :rules="rules" label="LoRa Version" :items="Versions" v-model="result.Devices[index].Version"></v-select>
                      <v-row  class="d-flex justify-center">
                        <v-btn :disabled="!vld(index)" @click="save(index)" class="lime lighten-4 black--text ma-5">Save</v-btn>
                        <v-btn @click="cancel(index)" class="lime lighten-4 black--text ma-5">Cancel</v-btn>
                      </v-row>
                    </v-card>
                  </v-container>
                </v-row>
<!-- End of User card addition for Edit -->
<!-- User card addition for Delete -->
                <v-row v-show="overlay_del[index]" class="justify-center">
                  <v-divider></v-divider>
                  <v-container>
                    <v-card class="d-flex justify-center flex-column pa-5">
                      <v-row>
                        <v-card-text>Are you sure to delete {{Device.DevName}}?</v-card-text>
                      </v-row>
                      <v-row  class="d-flex justify-center">
                        <v-btn @click="delDevice(index)" class="lime lighten-4 black--text ma-5">Delete</v-btn>
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
import '../assets/css/my.css'
// @ is an alias to /src
export default {
  data: () => ({
    result:null,
    DevName:null,
    DevEUI:null,
    AppKey:null,
    NwkKey:null,
    Version:null,
    overlay_add:false,
    overlay_edit:[],
    overlay_del:[],
    view_app:[],
    view_nwk:[],
    edit_app:[],
    edit_nwk:[],
    Versions: ['1.0','1.1'],
    show1: true,
    show2: true,
    rules: [v => v.length <=16 || 'Max 16 characters'],
    rules16: [ v => { const re=/^[0-9A-Fa-f]+$/
                     return re.test(v) || 'Only HEX digits'}, 
               v => v.length == 16 || 'Exactly 16 characters'],
    rules32: [v => v.length == 32 || '32 HEX characters'],
  }),
  mounted() {this.get()},
  computed: {
    validate :function() {
      return true
    }
  },
  methods : {
    click_app_down(index) {
      this.view_app[index]=true;
      this.$forceUpdate();
    },  
    click_app_up(index) {
      this.view_app[index]=false;
      this.$forceUpdate();
    },  
    click_nwk_down(index) {
      this.view_nwk[index]=true;
      this.$forceUpdate();
    },  
    click_nwk_up(index) {
      this.view_nwk[index]=false;
      this.$forceUpdate();
    },  
    vld(index) {
      const re=/^[0-9A-Fa-f]+$/;
      return (re.test(this.result.Devices[index].DevEUI) && re.test(this.result.Devices[index].AppKey) && re.test(this.result.Devices[index].NwkKey)
        && this.result.Devices[index].DevEUI.length==16 && this.result.Devices[index].AppKey.length==32 && this.result.Devices[index].NwkKey.length==32);
    },  
    get () {
      this.$ajax.get(this.$store.state.server+'/devices',{
        headers: {
          'Authorization': 'Bearer ' + this.$store.state.jwt_token
        }
      })
      .then(data => {
        this.result=data.data;
        this.result.Devices.forEach((currentValue,index) =>
        {
          this.result.Devices[index].Version=this.Versions[data.data.Devices[index].Version];
          this.overlay_edit[index]=false;
          this.overlay_del[index]=false;
          this.view_app[index]=false;
          this.view_nwk[index]=false;
          this.edit_app[index]=false;
          this.edit_nwk[index]=false;
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
    delDevice (index) {
      this.$ajax.post(this.$store.state.server+'/devices',{
        'DevEUI': this.result.Devices[index].DevEUI,
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
    edit (index) {
//      console.log('index='+index+' '+this.overlay_edit[index].toString());
      this.overlay_edit[index]=!this.overlay_edit[index];
      this.overlay_del[index]=false;
      this.overlay_add=false;
      this.$forceUpdate();
    },
    del (index) {
      this.overlay_edit[index]=false;
      this.overlay_del[index]=!this.overlay_del[index];
      this.overlay_add=false;
      this.$forceUpdate();
    },
    addDevice () {
      this.overlay_add=!this.overlay_add;
      this.$forceUpdate();
    },
    cancel (index) {
      this.overlay_edit[index]=false;
      this.overlay_del[index]=false;
      this.overlay_add=false;
      this.get();
   },
    add () {
      this.overlay_add=false;
//      console.log(this.FirstName.toString());
      this.$ajax.post(this.$store.state.server+'/devices',{
        'DevName': this.DevName,
        'DevEUI': this.DevEUI,
        'AppKey': this.AppKey,
        'NwkKey': this.NwkKey,
	'Version': this.Versions.findIndex(element => {return element===this.Version;}).toString()
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
    save (index) {
      this.overlay_edit[index]=false;
      this.$ajax.post(this.$store.state.server+'/devices',{
        'DevEUI': this.result.Devices[index].DevEUI,
        'DevName': this.result.Devices[index].DevName,
        'AppKey': this.result.Devices[index].AppKey,
        'NwkKey': this.result.Devices[index].NwkKey,
	'Version': this.Versions.findIndex(element => {return element===this.result.Devices[index].Version;}).toString()
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
