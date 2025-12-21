export class Camera {
    constructor() {
        this.camera = null;//相机实例
    }
    /**
     * 初始化相机
     * @param {HTMLCanvasElement} canvas 画布实例
     */
    init(canvas) {
        this.camera = new Filament.Camera(canvas)
    }
}