<template>
<v-container fluid class="red lighten-4 pa-5 d-flex justify-start align-start" fill-height>
  <v-card class="grey lighten-4">
    <v-container>
      <v-toolbar class="lime lighten-3 pb-5">
         <v-toolbar-title class="black--text text--secondary"><h4>Gateway Properties</h4></v-toolbar-title>
      </v-toolbar>
      <v-container>
        <v-row>
          <v-col cols="auto"><v-chip label class="grey lighten-4 text-h5">JoinEUI</v-chip></v-col>
          <v-col cols="auto"><v-chip label class="blue lighten-4 text-h6">{{result.JoinEUI}}</v-chip></v-col>
        </v-row>
      </v-container>
      <v-divider light></v-divider>
      <v-card class="ma-5 pa-5">
        <v-row dense justify="space-between" >
          <v-col cols="auto">
            <v-select label="Channel" v-model="result.CurChannel" :items="result.Channel" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-select label="Bandwidth" v-model="result.CurBW" :items="result.BW" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-select label="SpreadFactor" v-model="result.CurSF" :items="result.SF" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-select label="CRC" v-model="result.CurCRC" :items="result.CRC" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-select label="FEC" v-model="result.CurFEC" :items="result.FEC" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-select label="Power" v-model="result.CurPower" :items="result.Power" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-select label="Header Mode" v-model="result.CurHM" :items="result.HM" outlined></v-select>
          </v-col>
          <v-col cols="auto">
            <v-text-field label="Network ID" v-model="result.NetID" outlined></v-text-field>
          </v-col>
        </v-row>
        <v-row class="pb-5">
          <v-spacer></v-spacer>
          <v-btn @click="change" class="lime lighten-4 black--text">Change</v-btn>
          <v-spacer></v-spacer>
        </v-row>
      </v-card>
     </v-container>
   </v-card>
</v-container>
</template>

<script>
// @ is an alias to /src
export default {
  data: () => ({
    result:null,
    rules_chnum : [
      value => !!value || 'Required.',
      value => (value && value>=1 && value<=20 ) || '1-20'
    ]
  }),
  mounted() {
    this.$ajax.get(this.$store.state.server+'/settings',{
      headers: {
        'Authorization': 'Bearer ' + this.$store.state.jwt_token
      }
    })
    .then(data => {
      this.result=data.data;
      this.result.CurChannel=this.result.Channel[data.data.CurChannel];      
      this.result.CurBW=this.result.BW[data.data.CurBW];      
      this.result.CurSF=this.result.SF[data.data.CurSF];      
      this.result.CurCRC=this.result.CRC[data.data.CurCRC];      
      this.result.CurFEC=this.result.FEC[data.data.CurFEC]      
      this.result.CurPower=this.result.Power[data.data.CurPower]      
      this.result.CurHM=this.result.HM[data.data.CurHM]      
      console.log('response '+data.response.status);
    })
    .catch(e => {   
      if (e.response && e.response.status === 400) {
        this.error_message=e.response.data.error;
      }
      if (e.response && e.response.status === 401) {
         this.$router.push({ name: "login" });
      }
   })
  },
  methods : {
    change () {
      this.$ajax.post(this.$store.state.server+'/settings',{
          'CurChannel': this.result.Channel.findIndex(element => {return element===this.result.CurChannel;}).toString(),
          'CurBW': this.result.BW.findIndex(element => {return element===this.result.CurBW;}).toString(),
          'CurSF': this.result.SF.findIndex(element => {return element===this.result.CurSF;}).toString(),
          'CurCRC': this.result.CRC.findIndex(element => {return element===this.result.CurCRC;}).toString(),
          'CurFEC': this.result.FEC.findIndex(element => {return element===this.result.CurFEC;}).toString(),
          'CurPower': this.result.Power.findIndex(element => {return element===this.result.CurPower;}).toString(),
          'CurHM': this.result.HM.findIndex(element => {return element===this.result.CurHM;}).toString(),
          'NetID' : this.result.NetID.toString()
        },
        {
          headers: {
            'Authorization': 'Bearer ' + this.$store.state.jwt_token
          }
        }
      )
      .then ()
      .catch(e => {
        if (e.response && e.response.status === 400) {
          this.error_message=e.response.data.error;
        }
        if (e.response && e.response.status === 401) {
           this.$router.push({ name: "login" });
        } 
      })
    }
  }
}
</script>
