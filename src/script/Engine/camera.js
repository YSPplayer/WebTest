export class Camera {
    constructor(engine,scene) {
        this.engine = engine;//引擎实例
        this.scene = scene;//场景实例
        this.cameraEntity = null;//相机实体
        this.camera = null;//相机实例
    }
    create() {
        this.cameraEntity = Filament.EntityManager.get().create();
        this.camera = this.engine.createCamera(this.cameraEntity);
        this.view = this.engine.createView();//创建一个view对象
        this.view.setCamera(this.camera);//设置相机
        this.view.setScene(this.scene);//设置场景
        this.view.setTransparentPickingEnabled(true); //启用对透明对象的识取（鼠标点击检测）
        this.view.setDynamicResolutionOptions({ enabled: false });//禁用动态分辨率，始终以最高分辨率运行
        this.view.setPostProcessingEnabled(true);//启动抗锯齿、环境光遮蔽等后处理
        return true;
    }
    
}