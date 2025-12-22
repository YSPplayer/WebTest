class Light {
    constructor(engine) {
        this.engine = engine;//引擎实例
        this.light = null;//灯光实列
    }
    create() {
        this.light = Filament.EntityManager.get().create();
        Filament.LightManager.Builder(Filament.LightManager$Type.SUN)
        .intensity(this.baseLightIntensity)
        .direction(this.baseLightDirection)
        .color([1.0, 0.98, 0.95])
        .castShadows(true)
        .build(this.engine, this.light);
        return true;
    }
}