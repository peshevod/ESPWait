<template>
  <v-container fluid class="red lighten-4 d-flex align-start pa-5" fill-height>
    <v-row>
      <v-card class="elevation-6">
        <v-container>
          <v-row>
            <v-col><v-img :src="require('../assets/logo.png')" contain height="250" width="250" position="left center"></v-img></v-col>
            <v-col class="d-flex flex-column">
              <v-card-text class="ma-auto grey--text">IDF version: {{result.Version}}</v-card-text>
              <v-card-text class="ma-auto grey--text">Chip: {{result.Model}}</v-card-text>
              <v-card-text class="ma-auto grey--text">Revision: {{result.Revision}}</v-card-text>
              <v-card-text class="ma-auto grey--text">Cores: {{result.Cores}}</v-card-text>
              <v-spacer></v-spacer>
            </v-col>
          </v-row>
        </v-container>
      </v-card>
    </v-row>
  </v-container>
</template>

<script>
export default {
  data: () => ({
    result:null
  }),
  mounted() { 
    this.$ajax.get(this.$store.state.server+'/system',{
    })
    .then(data => {
      this.result=data.data;
    })
    .catch(e => {   
      if (e.response && e.response.status === 400) {
        this.error_message=e.response.data.error;
      }
    })
  }, };
</script>
