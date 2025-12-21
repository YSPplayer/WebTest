<template>
     <canvas ref = "show3dref" class = "show3d">
     </canvas>
</template>

<script setup>
 import { Engine } from '@/script/engine.js'
 import { onMounted, ref } from 'vue'
 const show3dref = ref(null)
 const engine = new Engine()
 onMounted(async ()=>{
    await engine.init(show3dref.value)
 })
</script>

<style scoped>
    .show3d {
        background-color:black;
    }
</style>