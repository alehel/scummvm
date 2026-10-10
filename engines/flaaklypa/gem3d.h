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
#ifndef FLAAKLYPA_GEM3D_H
#define FLAAKLYPA_GEM3D_H

#include "common/array.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/puzzledata.h"

namespace Flaaklypa {

/**
 * The small software 3D renderer the original uses for the gems of the
 * puzzle sub game ("Solines smykkeskrin"): meshes with per vertex normals,
 * a look-at camera, painter's algorithm, environment mapped textures and a
 * per face lighting term. Matrices are kept in the original's storage
 * order so the transforms can be mirrored line by line.
 */

struct Vec3 {
	float x, y, z;
	Vec3() : x(0), y(0), z(0) {}
	Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	Vec3 operator-(const Vec3 &o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 operator+(const Vec3 &o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
	Vec3 operator*(float f) const { return Vec3(x * f, y * f, z * f); }
	float dot(const Vec3 &o) const { return x * o.x + y * o.y + z * o.z; }
	Vec3 cross(const Vec3 &o) const;
	void normalize();
	/** The original's coordinate swap (x, y, z) -> (x, -z, y), applied to all data from the executable. */
	Vec3 swapped() const { return Vec3(x, -z, y); }
};

struct Mat4 {
	float m[16];
	void identity();
	/** out[i*4+j] = sum_k a[i*4+k] * b[k*4+j], as the original's matrix product. */
	static Mat4 mul(const Mat4 &a, const Mat4 &b);
	Vec3 transformPoint(const Vec3 &v) const;
	Vec3 rotate(const Vec3 &v) const;
	void lookAt(const Vec3 &eye, const Vec3 &target);
};

struct Texture3D {
	int shift;       ///< log2 of the width
	int mask;
	int w, h;
	Common::Array<uint32> pixels;   ///< 0x00RRGGBB

	Texture3D() : shift(0), mask(0), w(0), h(0) {}
	bool load(const Graphics::ManagedSurface *bitmap);
	uint32 at(int u, int v) const { return pixels[((v & mask) << shift) + (u & mask)]; }
};

struct Material3D {
	enum Type {
		kFlat = 2,         ///< lit colour only
		kTextured = 4,     ///< texture (spherical mapping) averaged with the environment map
		kLit = 0x20        ///< environment map plus the lit colour
	};
	int type;
	byte r, g, b;           ///< base colour (RGB565 precision like the original)
	float kd, ks, shininess;
	const Texture3D *envMap;
	const Texture3D *texture;

	Material3D() : type(kLit), r(0), g(0), b(0), kd(0.5f), ks(1.0f), shininess(100.0f), envMap(nullptr), texture(nullptr) {}
	void setColor(byte r_, byte g_, byte b_) { r = r_ & 0xf8; g = g_ & 0xfc; b = b_ & 0xf8; }
};

class Mesh3D {
public:
	struct Vertex {
		Vec3 pos, normal;
		float u, v;          ///< spherical texture coordinates in texels
		// per render
		Vec3 view, viewNormal;
		float sx, sy, sz;
	};
	struct Face {
		int idx[3];
		Vec3 normal;
		// per render
		float depth;
		Vec3 viewNormal;
	};

	Mesh3D(const GemMesh &def, const Texture3D *sphericalTexture);

	Common::Array<Vertex> verts;
	Common::Array<Face> faces;
};

struct Object3D {
	Mesh3D *mesh;
	Material3D *material;
	Vec3 scale, rotation, position;   ///< rotation in degrees
	bool visible;
	Mat4 matrix, rotMatrix;

	Object3D() : mesh(nullptr), material(nullptr), scale(1, 1, 1), visible(true) {}
	void updateMatrix();
};

class Scene3D {
public:
	Scene3D();

	void setCamera(const Vec3 &eye, const Vec3 &target);
	void addObject(Object3D *o) { _objects.push_back(o); }
	void clearObjects() { _objects.clear(); }

	/** Renders into the surface, whose centre is the projection centre. The caller clears it. */
	void render(Graphics::ManagedSurface &dst);

	/** Focal length of the 800x600 projection, used for the sprites too. */
	static const float kFocal;

private:
	typedef Mesh3D::Face Face;
	struct DrawFace {
		Object3D *obj;
		Face *face;
		float depth;
	};

	void drawFace(Graphics::ManagedSurface &dst, Object3D *o, Face *f);

	Mat4 _view;
	Common::Array<Object3D *> _objects;
	Common::Array<DrawFace> _drawList;
};

/** The original's 1-D noise generators (used for the wobble of the selected gem and the joker colour). */
float noiseHash(uint32 x);
float noiseSmooth(float x, float bias, float jitter, float amplitude);
float noiseCubic(float x, float bias, float jitter, float amplitude);

} // End of namespace Flaaklypa

#endif
