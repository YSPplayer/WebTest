// import * as Filament from '@/package/filament.js';

export class Engine {
    constructor() {
        this.engine = null;//引擎实例
        this.canvas = null;//画布实例
        this.scene = null;//场景实例
        this.camera = null;//相机实例
        this.renderer = null;//渲染器实例
        this.lights = null;//灯光实例
        this.materials = null;//材质实例
    }
    /**
     * 初始化引擎
     * @param {HTMLCanvasElement} canvas 画布实例
     */
    async init(canvas) {
        if (!Filament.isReady) {
            await new Promise((resolve) => Filament.init([], resolve));
        }
        this.engine = Filament.Engine.create(canvas)
        this.scene = this.engine.createScene()

    }

}