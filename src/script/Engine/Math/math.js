
export class Math {
   constructor() {
   }
    // 计算面法线
    static calculateFaceNormal(v1, v2, v3) {
        const edge1 = glMatrix.vec3.create();
        const edge2 = glMatrix.vec3.create();
        const normal = glMatrix.vec3.create();
        // 计算两条边面法线
        glMatrix.vec3.subtract(edge1, v2, v1);//v2 - v1
        glMatrix.vec3.subtract(edge2, v3, v1);//v3 - v1
        // 计算叉积，获取到垂直于面的向量
        glMatrix.vec3.cross(normal, edge1, edge2);
        return normal;
    }
    //计算法线
    static updateNormals(vertices, indices) {
        const vsize = vertices.length;
        const isize = indices.length;
        // 顶点数量
        const nsize = vsize / 3;
        // 创建法线数组
        const normalsv3 = new Array(nsize);
        // 初始化法线数组为零向量
        for (let i = 0; i < nsize; i++) {
            normalsv3[i] = glMatrix.vec3.fromValues(0.0, 0.0, 0.0);
        }
        // 遍历每个三角形，计算面法线并将其添加到顶点法线
        for (let i = 0; i < isize; i += 3) {
            // 获取三角形三个顶点的索引
            const idx1 = indices[i];
            const idx2 = indices[i + 1];
            const idx3 = indices[i + 2];
            
            // 获取三角形的三个顶点坐标
            const v1 = glMatrix.vec3.fromValues(
                vertices[idx1 * 3 + 0],
                vertices[idx1 * 3 + 1],
                vertices[idx1 * 3 + 2]
            );
            
            const v2 = glMatrix.vec3.fromValues(
                vertices[idx2 * 3 + 0],
                vertices[idx2 * 3 + 1],
                vertices[idx2 * 3 + 2]
            );
            
            const v3 = glMatrix.vec3.fromValues(
                vertices[idx3 * 3 + 0],
                vertices[idx3 * 3 + 1],
                vertices[idx3 * 3 + 2]
            );
            // 计算面法线
            const normal = Algorithm.calculateFaceNormal(v1, v2, v3);
            // 将法线添加到每个顶点
            glMatrix.vec3.add(normalsv3[idx1], normalsv3[idx1], normal);
            glMatrix.vec3.add(normalsv3[idx2], normalsv3[idx2], normal);
            glMatrix.vec3.add(normalsv3[idx3], normalsv3[idx3], normal);
        }
        const normals = new Float32Array(nsize * 3);
        // 归一化所有顶点法线并将其存储在输出数组中
        for (let i = 0; i < nsize; i++) {
            // 归一化法线向量
            glMatrix.vec3.normalize(normalsv3[i], normalsv3[i]);
            normals[i * 3 + 0] = normalsv3[i][0];
            normals[i * 3 + 1] = normalsv3[i][1];
            normals[i * 3 + 2] = normalsv3[i][2];
        }
        return normals;
    }
    //计算切线
    static updateTangents(vertexArray, normalArray) {
        const vertexCount = vertexArray.length / 3;
        const tangents = new Float32Array(vertexCount * 4);
        const up = glMatrix.vec3.fromValues(0, 1, 0);
        const fallback = glMatrix.vec3.fromValues(1, 0, 0);
        const normal = glMatrix.vec3.create();
        const tangent = glMatrix.vec3.create();
        const bitangent = glMatrix.vec3.create();
        const rotation = glMatrix.mat3.create();
        const orientation = glMatrix.quat.create();

        for (let i = 0; i < vertexCount; i++) {
            const nx = normalArray[i * 3];
            const ny = normalArray[i * 3 + 1];
            const nz = normalArray[i * 3 + 2];
            normal[0] = nx;
            normal[1] = ny;
            normal[2] = nz;
            if (glMatrix.vec3.length(normal) < 1e-6) {
                // fallback to pointing up to avoid NaNs
                glMatrix.vec3.set(normal, 0, 1, 0);
            } else {
                glMatrix.vec3.normalize(normal, normal);
            }

            glMatrix.vec3.cross(tangent, up, normal);
            if (glMatrix.vec3.length(tangent) < 1e-6) {
                glMatrix.vec3.cross(tangent, fallback, normal);
            }
            glMatrix.vec3.normalize(tangent, tangent);

            glMatrix.vec3.cross(bitangent, normal, tangent);
            glMatrix.vec3.normalize(bitangent, bitangent);

            rotation[0] = tangent[0];
            rotation[1] = tangent[1];
            rotation[2] = tangent[2];
            rotation[3] = bitangent[0];
            rotation[4] = bitangent[1];
            rotation[5] = bitangent[2];
            rotation[6] = normal[0];
            rotation[7] = normal[1];
            rotation[8] = normal[2];

            glMatrix.quat.fromMat3(orientation, rotation);
            glMatrix.quat.normalize(orientation, orientation);

            tangents[i * 4] = orientation[0];
            tangents[i * 4 + 1] = orientation[1];
            tangents[i * 4 + 2] = orientation[2];
            tangents[i * 4 + 3] = orientation[3];
        }

        return tangents;
    }
}