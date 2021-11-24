
<template>
  <v-container>
    <v-layout row wrap align-center>
    <v-flex xs12 sm4 lg4>
      <v-card>
        <v-row>
          <v-col>
            <v-img :src="require('../assets/logo.png')" contain height="250" width="250" position="left center"></v-img>
          </v-col>
          <v-col lg="4" align-self="start">
            <v-card-text>
              <div class="ma-auto">
                <span class="grey--text">IDF version: {{version}}</span>
                <br>
                <span class="grey--text">ESP cores: {{cores}}</span>
              </div>
            </v-card-text>
          </v-col>
        </v-row>
      </v-card>
    </v-flex>
    </v-layout>
  </v-container>
</template>

<script>
export default {
	
  data() {
    return {
	version: "5.1",
      cores: "2",
    }
  },
  mounted() {
    this.$router.push({ name: "login" });
  }
};
</script>
