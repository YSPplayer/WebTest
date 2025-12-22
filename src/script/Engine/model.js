export class Model {
    constructor(engine) {
        this.engine = engine;//引擎实列
        this.positionarray = null;//位置数组
        this.tangentsarray = null;//切线数组
        this.normalarray = null;//法线数组
        this.uvarray = null;//uv数组
        this.colorsarray = null;//颜色数组
        this.indicesarray = null;//索引数组
        this.entity = null;//filament实体实列
        this.bounds = null;//模型包围盒
        // this. = false;//模型是否存在
    }
    //检查模型实例是否存在
    isEmpty() {
        return this.entity  === null;
    }
    build() {
        //创建实体实列
        this.entity = Filament.EntityManager.get().create();
        const positionCount = this.positionarray.length / 3;
        const vertexBuffer = Filament.VertexBuffer.Builder().vertexCount(positionCount)
        .bufferCount(4)
        .attribute(
            Filament.VertexAttribute.POSITION,
            0,
            Filament.VertexBuffer$AttributeType.FLOAT3,
            0,
            12
        )
        .attribute(
            Filament.VertexAttribute.TANGENTS,
            1,
            Filament.VertexBuffer$AttributeType.FLOAT4,
            0,
            16
        )
        .attribute(
            Filament.VertexAttribute.COLOR,
            2,
            Filament.VertexBuffer$AttributeType.FLOAT3,
            0,
            12
        )
        .attribute(
            Filament.VertexAttribute.UV0,
            3,
            Filament.VertexBuffer$AttributeType.FLOAT2,
            0,
            8
        )
        .build(this.engine);
        vertexBuffer.setBufferAt(this.engine, 0, this.positionarray);
        vertexBuffer.setBufferAt(this.engine, 1, this.tangentsarray);
        vertexBuffer.setBufferAt(this.engine, 2, this.colorsarray);
        vertexBuffer.setBufferAt(this.engine, 3, this.uvarray);
        const indexType = this.indicesarray instanceof Uint16Array ? Filament.IndexBuffer$IndexType.USHORT : Filament.IndexBuffer$IndexType.UINT;
        const indexBuffer = Filament.IndexBuffer.Builder()
            .indexCount(this.indicesarray.length)
            .bufferType(indexType)
            .build(this.engine);
        indexBuffer.setBuffer(this.engine, this.indicesarray);
        Filament.RenderableManager.Builder(1).boundingBox(this.bounds)
        .geometry(
            0,
            Filament.RenderableManager$PrimitiveType.TRIANGLES,
            vertexBuffer,
            indexBuffer
        )
        .culling(false) //禁用背面剔除
        .castShadows(true) //阴影投射
        .receiveShadows(true) //阴影接收
        .build(this.engine, entity);
        return true;
    }
    
}