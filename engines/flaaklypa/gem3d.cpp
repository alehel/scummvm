/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "common/algorithm.h"
#include "common/util.h"

#include "flaaklypa/gem3d.h"

namespace Flaaklypa {

// ---- vectors and matrices ------------------------------------------------

Vec3 Vec3::cross(const Vec3 &o) const {
	return Vec3(o.z * y - z * o.y, z * o.x - o.z * x, x * o.y - o.x * y);
}

void Vec3::normalize() {
	float len = sqrtf(x * x + y * y + z * z);
	if (len > 0) {
		float f = 1.0f / len;
		x *= f;
		y *= f;
		z *= f;
	}
}

void Mat4::identity() {
	for (int i = 0; i < 16; i++)
		m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

Mat4 Mat4::mul(const Mat4 &a, const Mat4 &b) {
	Mat4 out;
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			float s = 0;
			for (int k = 0; k < 4; k++)
				s += a.m[i * 4 + k] * b.m[k * 4 + j];
			out.m[i * 4 + j] = s;
		}
	return out;
}

Vec3 Mat4::transformPoint(const Vec3 &v) const {
	return Vec3(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
	            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
	            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]);
}

Vec3 Mat4::rotate(const Vec3 &v) const {
	return Vec3(m[0] * v.x + m[4] * v.y + m[8] * v.z,
	            m[1] * v.x + m[5] * v.y + m[9] * v.z,
	            m[2] * v.x + m[6] * v.y + m[10] * v.z);
}

// The original's look-at matrix: z axis towards the target, y axis up.
void Mat4::lookAt(const Vec3 &eye, const Vec3 &target) {
	Vec3 f = target - eye;
	f.normalize();
	Vec3 u;
	if (f.x == 0 && f.z == 0)
		u = Vec3(-f.y - eye.x, -eye.y, -eye.z);
	else
		u = Vec3(target.x - eye.x, target.y + 1.0f - eye.y, target.z - eye.z);
	u = u - f * u.dot(f);
	u.normalize();
	Vec3 r = u.cross(f);
	r.normalize();

	identity();
	m[0] = r.x; m[4] = r.y; m[8] = r.z;
	m[1] = u.x; m[5] = u.y; m[9] = u.z;
	m[2] = f.x; m[6] = f.y; m[10] = f.z;
	m[12] = -r.dot(eye);
	m[13] = -u.dot(eye);
	m[14] = -f.dot(eye);
}

static Mat4 rotationX(float deg) {
	Mat4 r;
	r.identity();
	float s = sinf(deg * (float)M_PI / 180.0f), c = cosf(deg * (float)M_PI / 180.0f);
	r.m[9] = s;
	r.m[5] = c;
	r.m[6] = -s;
	r.m[10] = c;
	return r;
}

static Mat4 rotationY(float deg) {
	Mat4 r;
	r.identity();
	float s = sinf(deg * (float)M_PI / 180.0f), c = cosf(deg * (float)M_PI / 180.0f);
	r.m[0] = c;
	r.m[8] = -s;
	r.m[10] = c;
	r.m[2] = s;
	return r;
}

static Mat4 rotationZ(float deg) {
	Mat4 r;
	r.identity();
	float s = sinf(deg * (float)M_PI / 180.0f), c = cosf(deg * (float)M_PI / 180.0f);
	r.m[4] = s;
	r.m[0] = c;
	r.m[1] = -s;
	r.m[5] = c;
	return r;
}

// ---- textures ------------------------------------------------------------

bool Texture3D::load(const Graphics::ManagedSurface *bitmap) {
	if (!bitmap)
		return false;
	w = bitmap->w;
	h = bitmap->h;
	shift = 0;
	while ((1 << shift) < w)
		shift++;
	mask = (1 << shift) - 1;
	pixels.resize((1 << shift) * h);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			byte r, g, b;
			bitmap->format.colorToRGB(bitmap->getPixel(x, y), r, g, b);
			pixels[(y << shift) + x] = (r << 16) | (g << 8) | b;
		}
	return true;
}

// ---- meshes --------------------------------------------------------------

Mesh3D::Mesh3D(const GemMesh &def, const Texture3D *sphericalTexture) {
	verts.resize(def.vertCount);
	for (int i = 0; i < def.vertCount; i++) {
		Vertex &v = verts[i];
		v.pos = Vec3(def.verts[i * 3], def.verts[i * 3 + 1], def.verts[i * 3 + 2]).swapped();
		v.normal = Vec3();
		v.u = v.v = 0;
		if (sphericalTexture) {
			// Spherical mapping from the vertex direction (before centring,
			// as the original does it).
			float len = sqrtf(v.pos.x * v.pos.x + v.pos.y * v.pos.y + v.pos.z * v.pos.z);
			float nx = len > 0 ? v.pos.x / len : 0, ny = len > 0 ? v.pos.y / len : 0, nz = len > 0 ? v.pos.z / len : 0;
			float lat = asinf(CLIP(ny, -1.0f, 1.0f));
			float lenxz = sqrtf(nx * nx + nz * nz);
			float angle = 0;
			if (lenxz != 0) {
				if (nz == 0)
					angle = nx <= 0 ? (float)M_PI : 0.0f;
				else {
					angle = acosf(CLIP(nx / lenxz, -1.0f, 1.0f));
					if (nz < 0)
						angle = 2.0f * (float)M_PI - angle;
				}
				angle *= 0.159155f;
			}
			v.u = angle * sphericalTexture->w;
			v.v = (lat * 0.31831f + 0.5f) * sphericalTexture->h;
		}
	}
	faces.resize(def.faceCount);
	for (int i = 0; i < def.faceCount; i++) {
		Face &f = faces[i];
		for (int k = 0; k < 3; k++)
			f.idx[k] = def.faces[i * 3 + k];
		Vec3 e1 = verts[f.idx[0]].pos - verts[f.idx[1]].pos;
		Vec3 e2 = verts[f.idx[0]].pos - verts[f.idx[2]].pos;
		f.normal = e1.cross(e2);
		f.normal.normalize();
		for (int k = 0; k < 3; k++)
			verts[f.idx[k]].normal = verts[f.idx[k]].normal + f.normal;
	}
	for (auto &v : verts)
		v.normal.normalize();

	// Centre the mesh on its bounding box.
	Vec3 mn(1e30f, 1e30f, 1e30f), mx(-1e30f, -1e30f, -1e30f);
	for (auto &v : verts) {
		mn.x = MIN(mn.x, v.pos.x); mn.y = MIN(mn.y, v.pos.y); mn.z = MIN(mn.z, v.pos.z);
		mx.x = MAX(mx.x, v.pos.x); mx.y = MAX(mx.y, v.pos.y); mx.z = MAX(mx.z, v.pos.z);
	}
	Vec3 c = mx - (mx - mn) * 0.5f;
	for (auto &v : verts)
		v.pos = v.pos - c;
}

void Object3D::updateMatrix() {
	Mat4 ab = Mat4::mul(rotationX(rotation.x), rotationY(rotation.y));
	rotMatrix = Mat4::mul(rotationZ(rotation.z), ab);
	Mat4 s;
	s.identity();
	s.m[0] = scale.x;
	s.m[5] = scale.y;
	s.m[10] = scale.z;
	matrix = Mat4::mul(s, rotMatrix);
	matrix.m[12] = position.x;
	matrix.m[13] = position.y;
	matrix.m[14] = position.z;
}

// ---- scene ---------------------------------------------------------------

// 400 / (1 / tan(1.2042772769927979)): the original's projection for 800x600.
const float Scene3D::kFocal = 400.0f * 2.6046f;

Scene3D::Scene3D() {
	_view.identity();
}

void Scene3D::setCamera(const Vec3 &eye, const Vec3 &target) {
	_view.lookAt(eye, target);
}

static bool onScreen(float x, float y, int w, int h) {
	return x >= 0 && x <= w && y >= 0 && y <= h;
}

void Scene3D::render(Graphics::ManagedSurface &dst) {
	const int w = dst.w, h = dst.h;
	_drawList.clear();
	for (auto *o : _objects) {
		if (!o->visible || !o->mesh)
			continue;
		o->updateMatrix();
		Mat4 mv = Mat4::mul(o->matrix, _view);
		Mat4 rv = Mat4::mul(o->rotMatrix, _view);
		for (auto &v : o->mesh->verts) {
			v.view = mv.transformPoint(v.pos);
			v.viewNormal = rv.rotate(v.normal);
			float iz = v.view.z != 0 ? 1.0f / v.view.z : 0;
			v.sx = w * 0.5f + kFocal * v.view.x * iz;
			v.sy = h * 0.5f + kFocal * v.view.y * iz;
			v.sz = v.view.z;
		}
		for (auto &f : o->mesh->faces) {
			f.viewNormal = rv.rotate(f.normal);
			f.depth = (o->mesh->verts[f.idx[0]].sz + o->mesh->verts[f.idx[1]].sz + o->mesh->verts[f.idx[2]].sz) * 0.333333f;
			DrawFace d = { o, &f, f.depth };
			_drawList.push_back(d);
		}
	}
	// Painter's algorithm: far faces first.
	Common::sort(_drawList.begin(), _drawList.end(), [](const DrawFace &a, const DrawFace &b) { return a.depth > b.depth; });
	for (auto &d : _drawList) {
		const Mesh3D::Vertex *v[3];
		bool visible = true;
		for (int k = 0; k < 3; k++) {
			v[k] = &d.obj->mesh->verts[d.face->idx[k]];
			if (!onScreen(v[k]->sx, v[k]->sy, w, h))
				visible = false;
		}
		if (visible)
			drawFace(dst, d.obj, d.face);
	}
}

namespace {

struct RasterVertex {
	float x, y;
	float a[4];
};

/** Fills a triangle, calling fn(x, y, attributes) for every pixel inside it. */
template<typename Fn>
void rasterize(const RasterVertex *v, int w, int h, Fn fn) {
	float minX = MIN(v[0].x, MIN(v[1].x, v[2].x)), maxX = MAX(v[0].x, MAX(v[1].x, v[2].x));
	float minY = MIN(v[0].y, MIN(v[1].y, v[2].y)), maxY = MAX(v[0].y, MAX(v[1].y, v[2].y));
	int x0 = MAX(0, (int)floorf(minX)), x1 = MIN(w - 1, (int)ceilf(maxX));
	int y0 = MAX(0, (int)floorf(minY)), y1 = MIN(h - 1, (int)ceilf(maxY));
	float area = (v[1].x - v[0].x) * (v[2].y - v[0].y) - (v[2].x - v[0].x) * (v[1].y - v[0].y);
	if (area == 0)
		return;
	float inv = 1.0f / area;
	for (int y = y0; y <= y1; y++) {
		float py = y + 0.5f;
		for (int x = x0; x <= x1; x++) {
			float px = x + 0.5f;
			float w0 = ((v[1].x - px) * (v[2].y - py) - (v[2].x - px) * (v[1].y - py)) * inv;
			float w1 = ((v[2].x - px) * (v[0].y - py) - (v[0].x - px) * (v[2].y - py)) * inv;
			float w2 = 1.0f - w0 - w1;
			if (w0 < 0 || w1 < 0 || w2 < 0)
				continue;
			float a[4];
			for (int k = 0; k < 4; k++)
				a[k] = w0 * v[0].a[k] + w1 * v[1].a[k] + w2 * v[2].a[k];
			fn(x, y, a);
		}
	}
}

} // anonymous namespace

void Scene3D::drawFace(Graphics::ManagedSurface &dst, Object3D *o, Face *f) {
	const Mesh3D::Vertex *v[3];
	for (int k = 0; k < 3; k++)
		v[k] = &o->mesh->verts[f->idx[k]];
	// Back face test in screen space, as the original.
	if ((v[2]->sx - v[0]->sx) * (v[1]->sy - v[0]->sy) - (v[1]->sx - v[0]->sx) * (v[2]->sy - v[0]->sy) <= 0)
		return;

	const Material3D *mat = o->material;
	const Texture3D *env = mat->envMap;
	const Graphics::PixelFormat &fmt = dst.format;
	RasterVertex rv[3];
	for (int k = 0; k < 3; k++) {
		rv[k].x = v[k]->sx;
		rv[k].y = v[k]->sy;
		// Environment mapping: the transformed normal picks the texel.
		rv[k].a[0] = env ? (v[k]->viewNormal.x + 1.0f) * (env->w / 2) : 0;
		rv[k].a[1] = env ? (v[k]->viewNormal.y + 1.0f) * (env->h / 2) : 0;
		rv[k].a[2] = v[k]->u;
		rv[k].a[3] = v[k]->v;
	}

	if (mat->type == Material3D::kTextured && mat->texture && env) {
		// Gold and silver: the environment map (spherically mapped) averaged
		// with the metal texture (environment mapped).
		const Texture3D *tex = mat->texture;
		rasterize(rv, dst.w, dst.h, [&](int x, int y, const float *a) {
			uint32 t1 = env->at((int)a[2], (int)a[3]);
			uint32 t2 = tex->at((int)a[0], (int)a[1]);
			byte r = (byte)((((t1 >> 16) & 0xff) + ((t2 >> 16) & 0xff)) >> 1);
			byte g = (byte)((((t1 >> 8) & 0xff) + ((t2 >> 8) & 0xff)) >> 1);
			byte b = (byte)(((t1 & 0xff) + (t2 & 0xff)) >> 1);
			dst.setPixel(x, y, fmt.RGBToColor(r, g, b));
		});
		return;
	}

	// Lit colour: the face normal's view space z is the light term.
	float r = mat->r, g = mat->g, b = mat->b;
	if (mat->kd > 0 || mat->ks > 0) {
		float intensity = -0.5f * f->viewNormal.z + 0.5f;
		if (mat->kd > 0) {
			float t = intensity * mat->kd;
			r *= t; g *= t; b *= t;
		}
		if (mat->ks > 0) {
			float s = powf(intensity, mat->shininess) * mat->ks * 255.0f;
			r += s; g += s; b += s;
		}
		r = CLIP(r, 0.0f, 200.0f);
		g = CLIP(g, 0.0f, 200.0f);
		b = CLIP(b, 0.0f, 200.0f);
	}
	int cr = (int)r, cg = (int)g, cb = (int)b;

	if (mat->type == Material3D::kFlat || !env) {
		uint32 color = fmt.RGBToColor(cr, cg, cb);
		rasterize(rv, dst.w, dst.h, [&](int x, int y, const float *) { dst.setPixel(x, y, color); });
		return;
	}
	// Environment map plus the lit colour (saturating add).
	rasterize(rv, dst.w, dst.h, [&](int x, int y, const float *a) {
		uint32 t = env->at((int)a[0], (int)a[1]);
		int pr = MIN(255, (int)((t >> 16) & 0xff) + cr);
		int pg = MIN(255, (int)((t >> 8) & 0xff) + cg);
		int pb = MIN(255, (int)(t & 0xff) + cb);
		dst.setPixel(x, y, fmt.RGBToColor(pr, pg, pb));
	});
}

// ---- noise ---------------------------------------------------------------

float noiseHash(uint32 x) {
	x = x ^ (x << 13);
	uint32 n = (x * x * 0x3d73 + 0xc0ae5) * x + 0xd208dd0d;
	return (float)(n & 0x7fffffff) * 4.656612873e-10f;
}

// A 1-D noise with randomly jittered segment lengths; returns a value in
// [0, 1] that goes up and down between the lattice points.
float noiseSmooth(float x, float bias, float jitter, float amplitude) {
	int i0 = (int)floorf(x);
	int i1 = i0 + 1;
	float f = x - (float)i0;
	int a = i0, b = i1;
	if (jitter != 0) {
		float lo = (noiseHash(i0 * 0x11) - 0.5f) * jitter;
		float hi = (noiseHash(i1 * 0x11) - 0.5f) * jitter + 1.0f;
		if (lo <= f) {
			if (hi < f) {
				f -= 1.0f;
				b = i0 + 2;
				lo = hi - 1.0f;
				hi = (noiseHash(b * 0x11) - 0.5f) * jitter + 1.0f;
				a = i1;
			}
		} else {
			f += 1.0f;
			float hi2 = lo + 1.0f;
			lo = (noiseHash((i0 - 1) * 0x11) - 0.5f) * jitter;
			hi = hi2;
			a = i0 - 1;
			b = i0;
		}
		f = (f - lo) / (hi - lo);
	}
	if (a & 1) {
		f = 1.0f - f;
		a++;
		b--;
	}
	if (bias >= 0) {
		if (bias > 0)
			f = f / ((1.0f - bias) + f * bias);
	} else {
		f = ((bias + 1.0f) * f) / (f * bias + 1.0f);
	}
	if (amplitude == 0)
		return f;
	float va = noiseHash(a * 0x61) * amplitude * 0.5f;
	float vb = 1.0f - va;
	float vc = noiseHash(b * 0x61) * amplitude * 0.5f;
	return (vb - vc) * f + va;
}

float noiseCubic(float x, float bias, float jitter, float amplitude) {
	// Same as noiseSmooth with a smoothstep applied before the amplitude.
	float amp = amplitude;
	float f = noiseSmooth(x, bias, jitter, 0);
	f = (3.0f - 2.0f * f) * f * f;
	if (amp == 0)
		return f;
	// Recompute the lattice points the way noiseSmooth does.
	int i0 = (int)floorf(x);
	int a = i0, b = i0 + 1;
	if (jitter != 0) {
		float fr = x - (float)i0;
		float lo = (noiseHash(i0 * 0x11) - 0.5f) * jitter;
		float hi = (noiseHash((i0 + 1) * 0x11) - 0.5f) * jitter + 1.0f;
		if (lo <= fr) {
			if (hi < fr) {
				a = i0 + 1;
				b = i0 + 2;
			}
		} else {
			a = i0 - 1;
			b = i0;
		}
	}
	if (a & 1) {
		a++;
		b--;
	}
	float va = noiseHash(a * 0x61) * amp * 0.5f;
	float vb = 1.0f - va;
	float vc = noiseHash(b * 0x61) * amp * 0.5f;
	return (vb - vc) * f + va;
}

} // End of namespace Flaaklypa
