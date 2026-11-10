"""Apply native-authored bounded indexed geometry; shared by boxes and polygon products."""
import bpy
import math
from collections import Counter
from mathutils import Matrix, Vector

def normalize_prototype_meshes(meshes, target_height):
    """Normalize actual world-baked vertices, not rotated conservative boxes."""
    if type(target_height) not in (int, float) or not math.isfinite(target_height) or not 0.01 <= target_height <= 1000:
        raise ValueError("Prototype height is outside metric bounds")
    low, high = Vector((math.inf,) * 3), Vector((-math.inf,) * 3)
    count = 0
    for mesh in meshes:
        count += len(mesh.vertices)
        if count > 2000000:
            raise ValueError("Prototype vertex budget exceeded")
        for vertex in mesh.vertices:
            for axis in range(3):
                value = vertex.co[axis]
                if not math.isfinite(value):
                    raise ValueError("Prototype vertex is not finite")
                low[axis], high[axis] = min(low[axis], value), max(high[axis], value)
    if count == 0 or high.z - low.z <= 1e-8:
        raise ValueError("Prototype has no positive geometric height")
    center = Vector(((low.x + high.x) / 2, (low.y + high.y) / 2, low.z))
    normalize = Matrix.Scale(target_height / (high.z - low.z), 4) @ Matrix.Translation(-center)
    for mesh in meshes:
        mesh.transform(normalize)
        mesh.update()

def indexed_mesh(spec, vertices, faces):
    topology = spec.get("topology", "closed")
    if topology not in ("closed", "open_surface"):
        raise ValueError("Unsupported indexed topology")
    opened = topology == "open_surface"
    if not 3 <= len(vertices) <= (4096 if opened else 512) or not (1 if opened else 4) <= len(faces) <= (8192 if opened else 768):
        raise ValueError("Indexed geometry budget exceeded")
    if opened and spec["bevel"] != 0:
        raise ValueError("Open simulation surfaces cannot alter topology with bevel")
    if any(len(p) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 10000 for v in p) for p in vertices):
        raise ValueError("Invalid indexed mesh vertex")
    directed = Counter()
    links = [{} for _ in vertices]
    for face in faces:
        if len(face) not in (3, 4) or len(set(face)) != len(face):
            raise ValueError("Invalid indexed mesh face")
        if any(type(i) is not int or not 0 <= i < len(vertices) for i in face):
            raise ValueError("Indexed mesh vertex reference out of range")
        for a, b in zip(face, face[1:] + face[:1]):
            directed[(a, b)] += 1
        for index, vertex in enumerate(face):
            a, b = face[index-1], face[(index+1) % len(face)]
            links[vertex].setdefault(a, set()).add(b)
            links[vertex].setdefault(b, set()).add(a)
    if any(count != 1 or (not opened and directed[(b, a)] != 1) for (a, b), count in list(directed.items())):
        raise ValueError("Indexed mesh is not closed with consistent face winding")
    if opened:
        if not any((b, a) not in directed for a, b in directed):
            raise ValueError("Open surface must have a boundary")
        for link in links:
            if not link or any(len(neighbors) > 2 for neighbors in link.values()):
                raise ValueError("Open surface contains a non-manifold vertex")
            pending, visited = [next(iter(link))], set()
            while pending:
                vertex = pending.pop()
                if vertex not in visited:
                    visited.add(vertex)
                    pending.extend(link[vertex] - visited)
            if len(visited) != len(link):
                raise ValueError("Open surface contains disconnected vertex fans")
    mesh = bpy.data.meshes.new(spec["id"])
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    smooth = spec.get("smooth", False)
    if type(smooth) is not bool:
        raise ValueError("Indexed mesh smooth shading must be boolean")
    for polygon in mesh.polygons:
        polygon.use_smooth = smooth
    # Canonical boxes carry metric UVs; material tile size is authored in C++.
    uv = mesh.uv_layers.new(name="MetricUV")
    for polygon in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(polygon.normal[i]))
        axes = ((1, 2), (0, 2), (0, 1))[axis]
        basis = [Vector(tuple(float(i == selected) for i in range(3))) for selected in axes]
        u = (basis[0] - polygon.normal * basis[0].dot(polygon.normal)).normalized()
        v = basis[1] - polygon.normal * basis[1].dot(polygon.normal)
        v = (v - u * v.dot(u)).normalized()
        for loop_index in polygon.loop_indices:
            # Native boxes/meshes have translation-only placement. Bake the same
            # metric texture phase across adjacent facade pieces, not per-object.
            point = mesh.vertices[mesh.loops[loop_index].vertex_index].co + Vector(spec["location"])
            uv.data[loop_index].uv = (point.dot(u), point.dot(v))
    obj = bpy.data.objects.new(spec["id"], mesh)
    bpy.context.scene.collection.objects.link(obj)
    if spec["bevel"] > 0:
        modifier = obj.modifiers.new("Physical edge bevel", "BEVEL")
        modifier.width, modifier.segments = spec["bevel"], 2
    return obj

def box(spec):
    x, y, z = (v / 2 for v in spec["size"])
    vertices = [(-x,-y,-z), (x,-y,-z), (x,y,-z), (-x,y,-z),
                (-x,-y,z), (x,-y,z), (x,y,z), (-x,y,z)]
    faces = [(0,3,2,1), (4,5,6,7), (0,1,5,4), (1,2,6,5), (2,3,7,6), (3,0,4,7)]
    return indexed_mesh(spec, vertices, faces)
