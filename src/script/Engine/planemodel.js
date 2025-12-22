import {Model} from './model.js'
import {Math} from '../Math/math.js'
export class PlaneModel extends Model {
    constructor(engine) {
        super(engine)
    }
    create(mode) {
        if(mode === 'enginePlane') { //引擎地面
            this.createEnginePlane()
        }
        return super.build()
    }
    createEnginePlane() {
        this.positionarray = new Float32Array([
            -1, 0, 1,
            1, 0, 1,
            1, 0, -1,
            -1, 0, -1
        ])
       this.indicesarray = new Uint16Array([
        0, 1, 2,
        0, 2, 3
       ])
       this.normalarray = Math.updateNormals(this.positionarray, this.indicesarray)
       this.tangentsarray = Math.updateTangents(this.positionarray, this.normalarray)
       this.bounds = Math.updateBounds(this.positionarray)
       this.uvarray = null
       this.colorsarray = null
    }
}