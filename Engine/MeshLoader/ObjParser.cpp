#include <StdAfx.h>
#include <Engine/Mesh.h>
#include <Engine/TriangleData.h>
#include <Engine/Material.h>
#include <Engine/TriIntersectorData.h>
#include <filesystem.h>
#include <Base/FileStream.h>
#include <Base/Platform.h>
#include <unordered_map>
#include <iterator>

template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
namespace std
{
template<> struct hash<CudaTracerLib::Vec3i>
{
	size_t operator()(const CudaTracerLib::Vec3i & e) const
	{
		std::size_t seed = 0;
		hash_combine(seed, e.x);
		hash_combine(seed, e.y);
		hash_combine(seed, e.z);
		return seed;
	}
};
}

namespace CudaTracerLib {

bool operator==(const Vec3i &v0, const Vec3i &v1)
{
	return (
		v0.x == v1.x &&
		v0.y == v1.y &&
		v0.z == v1.z
		);
}

class VertexHash
{
	std::unordered_map<Vec3i, int> entries;
public:
	bool search(const Vec3i& key, int& val)
	{
		auto a = entries.find(key);
		if (a == entries.end())
			return false;
		else
		{
			if (a->first != key)
				throw std::runtime_error("Error hashing obj vertex index tuples!");//safeguard against older hashing strategies
			val = a->second;
			return true;
		}
	}
	int add(const Vec3i& key, int value)
	{
		entries[key] = value;
		return value;
	}
};

bool parseSpace(const char*& ptr)
{
	while (*ptr == ' ' || *ptr == '\t')
		ptr++;
	return true;
}

bool parseChar(const char*& ptr, char chr)
{
	if (*ptr != chr)
		return false;
	ptr++;
	return true;
}

bool parseLiteral(const char*& ptr, const char* str)
{
	const char* tmp = ptr;

	while (*str && *tmp == *str)
	{
		tmp++;
		str++;
	}
	if (*str)
		return false;

	ptr = tmp;
	return true;
}

bool parseInt(const char*& ptr, int& value)
{
	const char* tmp = ptr;
	int v = 0;
	bool neg = (!parseChar(tmp, '+') && parseChar(tmp, '-'));
	if (*tmp < '0' || *tmp > '9')
		return false;
	while (*tmp >= '0' && *tmp <= '9')
		v = v * 10 + *tmp++ - '0';

	value = (neg) ? -v : v;
	ptr = tmp;
	return true;
}

bool parseInt(const char*& ptr, long long& value)
{
	const char* tmp = ptr;
	long long v = 0;
	bool neg = (!parseChar(tmp, '+') && parseChar(tmp, '-'));
	if (*tmp < '0' || *tmp > '9')
		return false;
	while (*tmp >= '0' && *tmp <= '9')
		v = v * 10 + *tmp++ - '0';

	value = (neg) ? -v : v;
	ptr = tmp;
	return true;
}

bool parseHex(const char*& ptr, unsigned int& value)
{
	const char* tmp = ptr;
	unsigned int v = 0;
	for (;;)
	{
		if (*tmp >= '0' && *tmp <= '9')         v = v * 16 + *tmp++ - '0';
		else if (*tmp >= 'A' && *tmp <= 'F')    v = v * 16 + *tmp++ - 'A' + 10;
		else if (*tmp >= 'a' && *tmp <= 'f')    v = v * 16 + *tmp++ - 'a' + 10;
		else                                    break;
	}

	if (tmp == ptr)
		return false;

	value = v;
	ptr = tmp;
	return true;
}

bool parseFloat(const char*& ptr, float& value)
{
	const char* tmp = ptr;
	bool neg = (!parseChar(tmp, '+') && parseChar(tmp, '-'));

	float v = 0.0f;
	int numDigits = 0;
	while (*tmp >= '0' && *tmp <= '9')
	{
		v = v * 10.0f + (float)(*tmp++ - '0');
		numDigits++;
	}
	if (parseChar(tmp, '.'))
	{
		float scale = 1.0f;
		while (*tmp >= '0' && *tmp <= '9')
		{
			scale *= 0.1f;
			v += scale * (float)(*tmp++ - '0');
			numDigits++;
		}
	}
	if (!numDigits)
		return false;

	ptr = tmp;
	if (*ptr == '#')
	{
		unsigned int v = 0;
		if (parseLiteral(ptr, "#INF"))
			v = 0x7F800000;
		else if (parseLiteral(ptr, "#SNAN"))
			v = 0xFF800001;
		else if (parseLiteral(ptr, "#QNAN"))
			v = 0xFFC00001;
		else if (parseLiteral(ptr, "#IND"))
			v = 0xFFC00000;
		if (v)
		{
			v |= neg << 31;
			value = *(float*)&v;
			return true;
		}
		else return false;
	}

	int e = 0;
	if ((parseChar(tmp, 'e') || parseChar(tmp, 'E')) && parseInt(tmp, e))
	{
		ptr = tmp;
		if (e)
			v *= pow(10.0f, (float)e);
	}
	value = (neg) ? -v : v;
	return true;
}

enum TextureType
{
	TextureType_Diffuse = 0,    // Diffuse color map.
	TextureType_Specular,
	TextureType_Alpha,          // Alpha map (green = opacity).
	TextureType_Displacement,   // Displacement map (green = height).
	TextureType_Normal,         // Tangent-space normal map.
	TextureType_Environment,    // Environment map (spherical coordinates).

	TextureType_Max
};

struct ObjMaterial
{
	std::string		Name;
	int				IlluminationModel;
	Vec4f          diffuse;
	Vec3f          specular;
	Vec3f			emission;
	float           glossiness;
	float           displacementCoef; // height = texture/255 * coef + bias
	float           displacementBias;
	float			IndexOfRefraction;
	Vec3f			Tf;
	std::string     textures[TextureType_Max];
	int				submesh;

	ObjMaterial(void)
	{
		diffuse = Vec4f(0.75f, 0.75f, 0.75f, 1.0f);
		specular = Vec3f(0.5f);
		glossiness = 32.0f;
		displacementCoef = 1.0f;
		displacementBias = 0.0f;
		emission = Vec3f(0, 0, 0);
		submesh = -1;
		IlluminationModel = 2;
	}
};

struct TextureSpec
{
	std::string	              texture;
	float                     base;
	float                     gain;
};

struct MatHash
{
	std::vector<ObjMaterial> vec;
	std::map<std::string, int> map;

	void add(const std::string& name, const ObjMaterial& mat)
	{
		map[name] = (int)vec.size();
		vec.push_back(mat);
	}

	bool contains(const std::string& name)
	{
		return map.find(name) != map.end();
	}

	int searchi(const std::string& name)
	{
		std::map<std::string, int>::iterator it = map.find(name);
		return it == map.end() ? -1 : it->second;
	}
};

bool parseFloats(const char*& ptr, float* values, int num)
{
	const char* tmp = ptr;
	for (int i = 0; i < num; i++)
	{
		if (i)
			parseSpace(tmp);
		if (!parseFloat(tmp, values[i]))
			return false;
	}
	ptr = tmp;
	return true;
}

bool parseTexture(const char*& ptr, TextureSpec& value, const std::string& dirName)
{
	// Initialize result.

	std::string name;
	value.texture = "";
	value.base = 0.0f;
	value.gain = 1.0f;

	// Parse options.

	while (*ptr)
	{
		parseSpace(ptr);
		if ((parseLiteral(ptr, "-blendu ") || parseLiteral(ptr, "-blendv ") || parseLiteral(ptr, "-cc ") || parseLiteral(ptr, "-math::clamp ")) && parseSpace(ptr))
		{
			if (!parseLiteral(ptr, "on") && !parseLiteral(ptr, "off"))
				return false;
		}
		else if (parseLiteral(ptr, "-mm ") && parseSpace(ptr))
		{
			if (!parseFloat(ptr, value.base) || !parseSpace(ptr) || !parseFloat(ptr, value.gain))
				return false;
		}
		else if ((parseLiteral(ptr, "-o ") || parseLiteral(ptr, "-s ") || parseLiteral(ptr, "-t ")) && parseSpace(ptr))
		{
			float tmp[2];
			if (!parseFloats(ptr, tmp, 2))
				return false;
			parseSpace(ptr);
			parseFloat(ptr, tmp[0]);
		}
		else if ((parseLiteral(ptr, "-texres ") || parseLiteral(ptr, "-bm ")) && parseSpace(ptr))
		{
			float tmp;
			if (!parseFloat(ptr, tmp))
				return false;
		}
		else if (parseLiteral(ptr, "-type ") && parseSpace(ptr))
		{
			if (!parseLiteral(ptr, "sphere") &&
				!parseLiteral(ptr, "cube_top") && !parseLiteral(ptr, "cube_bottom") &&
				!parseLiteral(ptr, "cube_front") && !parseLiteral(ptr, "cube_back") &&
				!parseLiteral(ptr, "cube_left") && !parseLiteral(ptr, "cube_right"))
			{
				return false;
			}
		}
		else
		{
			if (*ptr == '-' || name.size())
				return false;
			while (*ptr && (*ptr != '-' || !ends_with(name, " ")))
				name += *ptr++;
		}
	}

	// Process file name.

	while (starts_with(name, "/"))
		name = name.substr(1);
	while (ends_with(name, " "))
		name = name.substr(0, name.size() - 1);

	// Zero-length file name => ignore.

	if (!name.size())
		return true;

	// Import texture.

	value.texture = dirName + '/' + name;

	return true;
}

std::string get_texture_path(const std::string& path, const std::string& dirName)
{
	if (std::filesystem::exists(dirName + "/" + path))
		return dirName + "/" + path;
	else return path;
}

struct SubMesh
{
	std::vector<Vec3i>   indices;
	ObjMaterial        material;

	SubMesh()
	{
		material.submesh = -1234;
	}
};

struct ImportState
{
	std::vector<SubMesh> subMeshes;

	std::vector<Vec3f>            positions;
	std::vector<Vec2f>            texCoords;
	std::vector<Vec3f>            normals;

	VertexHash        vertexHash;
	MatHash materialHash;

	std::vector<int>              vertexTmp;
	std::vector<Vec3i>            indexTmp;

	struct VertexPNT
	{
		Vec3f p;
		Vec2f t;
		Vec3f n;
	};
	std::vector<VertexPNT> vertices;

	int addVertex()
	{
		vertices.push_back(VertexPNT());
		return (int)vertices.size() - 1;
	}

	int addSubMesh()
	{
		subMeshes.push_back(SubMesh());
		return (int)subMeshes.size() - 1;
	}

	unsigned int numTriangles()
	{
		size_t n = 0;
		for (auto& s : subMeshes)
			n += s.indices.size();
		return (unsigned int)n;
	}
};

void loadMtl(ImportState& s, IInStream& mtlIn, const std::string& dirName)
{
	char ptrLast[256];
	ObjMaterial mat;
	bool hasMat = false;
	std::string lineS;
	while (mtlIn.getline(lineS))
	{
		trim(lineS);
		const char* ptr = lineS.c_str();
		parseSpace(ptr);
		bool valid = false;

		if (!*ptr || parseLiteral(ptr, "#"))
		{
			valid = true;
		}
		else if (parseLiteral(ptr, "newmtl ") && parseSpace(ptr) && *ptr) // material name
		{
			if (hasMat)
				s.materialHash.add(std::string(ptrLast), mat);
			if (!s.materialHash.contains(std::string(ptr)))
			{
				mat = ObjMaterial();
				hasMat = true;
				Platform::SetMemory(ptrLast, sizeof(ptrLast));
				memcpy(ptrLast, ptr, strlen(ptr));
				mat.Name = std::string(ptrLast);
			}
			valid = true;
		}
		else if (parseLiteral(ptr, "Ka ") && parseSpace(ptr)) // ambient color
		{
			float tmp[3];
			if (parseLiteral(ptr, "spectral ") || parseLiteral(ptr, "xyz "))
				valid = true;
			else if (parseFloats(ptr, tmp, 3) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "Kd ") && parseSpace(ptr)) // diffuse color
		{
			if (parseLiteral(ptr, "spectral ") || parseLiteral(ptr, "xyz "))
				valid = true;
			else if (parseFloats(ptr, (float*)&mat.diffuse, 3) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "Ks ") && parseSpace(ptr)) // specular color
		{
			if (parseLiteral(ptr, "spectral ") || parseLiteral(ptr, "xyz "))
				valid = true;
			else if (parseFloats(ptr, (float*)&mat.specular, 3) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "d ") && parseSpace(ptr)) // alpha
		{
			if (parseFloat(ptr, mat.diffuse.w) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "Ns ") && parseSpace(ptr)) // glossiness
		{
			if (parseFloat(ptr, mat.glossiness) && parseSpace(ptr) && !*ptr)
				valid = true;
			if (mat.glossiness <= 0.0f)
			{
				mat.glossiness = 1.0f;
				mat.specular = Vec3f(0);
			}
		}
		else if (parseLiteral(ptr, "map_Kd ")) // diffuse texture
		{
			TextureSpec tex;
			mat.textures[TextureType_Diffuse] = get_texture_path(std::string(ptr), dirName);
			valid = parseTexture(ptr, tex, dirName);
		}
		else if (parseLiteral(ptr, "map_Ks ")) // specular texture
		{
			TextureSpec tex;
			mat.textures[TextureType_Specular] = get_texture_path(std::string(ptr), dirName);
			valid = parseTexture(ptr, tex, dirName);
		}
		else if (parseLiteral(ptr, "Ke "))
		{
			if (parseFloats(ptr, (float*)&mat.emission, 3) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "Tf "))
		{
			if (parseFloats(ptr, (float*)&mat.Tf, 3) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "Ni ") && parseSpace(ptr)) // alpha
		{
			if (parseFloat(ptr, mat.IndexOfRefraction) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "illum ") && parseSpace(ptr)) // alpha
		{
			if (parseInt(ptr, mat.IlluminationModel) && parseSpace(ptr) && !*ptr)
				valid = true;
		}
		else if (parseLiteral(ptr, "map_d ") || parseLiteral(ptr, "map_D ") || parseLiteral(ptr, "map_opacity ")) // alpha texture
		{
			TextureSpec tex;
			mat.textures[TextureType_Alpha] = get_texture_path(std::string(ptr), dirName);
			valid = parseTexture(ptr, tex, dirName);
		}
		else if (parseLiteral(ptr, "disp ")) // displacement map
		{
			TextureSpec tex;
			mat.displacementCoef = tex.gain;
			mat.displacementBias = tex.base * tex.gain;
			mat.textures[TextureType_Displacement] = get_texture_path(std::string(ptr), dirName);
			valid = parseTexture(ptr, tex, dirName);
		}
		else if (parseLiteral(ptr, "bump ") || parseLiteral(ptr, "map_bump ") || parseLiteral(ptr, "map_Bump ")) // bump map
		{
			TextureSpec tex;
			mat.displacementCoef = tex.gain;
			mat.displacementBias = tex.base * tex.gain;
			mat.textures[TextureType_Displacement] = get_texture_path(std::string(ptr), dirName);
			valid = parseTexture(ptr, tex, dirName);
		}
		else if (parseLiteral(ptr, "refl ")) // environment map
		{
			TextureSpec tex;
			mat.textures[TextureType_Environment] = get_texture_path(std::string(ptr), dirName);
			valid = parseTexture(ptr, tex, dirName);
		}
		else if (
			parseLiteral(ptr, "vp ") ||             // parameter space vertex
			parseLiteral(ptr, "Kf ") ||             // transmission color
			parseLiteral(ptr, "illum ") ||          // illumination model
			parseLiteral(ptr, "d -halo ") ||        // orientation-dependent alpha
			parseLiteral(ptr, "sharpness ") ||      // reflection sharpness
			parseLiteral(ptr, "Ni ") ||             // index of refraction
			parseLiteral(ptr, "map_Ks ") ||         // specular texture
			parseLiteral(ptr, "map_kS ") ||         // ???
			parseLiteral(ptr, "map_kA ") ||         // ???
			parseLiteral(ptr, "map_Ns ") ||         // glossiness texture
			parseLiteral(ptr, "map_aat ") ||        // texture antialiasing
			parseLiteral(ptr, "decal ") ||          // blended texture
			parseLiteral(ptr, "Km ") ||             // ???
			parseLiteral(ptr, "Tr ") ||             // ???
			parseLiteral(ptr, "Ke ") ||             // ???
			parseLiteral(ptr, "pointgroup ") ||     // ???
			parseLiteral(ptr, "pointdensity ") ||   // ???
			parseLiteral(ptr, "smooth") ||          // ???
			parseLiteral(ptr, "R "))                // ???
		{
			valid = true;
		}
	}
	if (hasMat)
		s.materialHash.add(std::string(ptrLast), mat);
}

static Texture CreateTexture(const char* p, const Spectrum& col)
{
	if (p && *p)
		return CreateTexture(p);
	else return CreateTexture(col);
}

template<typename T> void push(std::vector<T>& left, const std::vector<T>& right)
{
	std::move(right.begin(), right.end(), std::back_inserter(left));
}

void parse(ImportState& s, IInStream& in)
{
	std::string dirName = std::filesystem::path(in.getFilePath()).parent_path().string();
	int submesh = -1;
	int defaultSubmesh = -1;
	std::string line;
	while (in.getline(line))
	{
		trim(line);
		const char* ptr = line.c_str();
		parseSpace(ptr);
		bool valid = false;

		if (!*ptr || parseLiteral(ptr, "#"))
		{
			valid = true;
		}
		else if (parseLiteral(ptr, "v ") && parseSpace(ptr)) // position vertex
		{
			Vec3f v;
			if (parseFloats(ptr, v.getPtr(), 3) && parseSpace(ptr) && !*ptr)
			{
				s.positions.push_back(v);
				valid = true;
			}
		}
		else if (parseLiteral(ptr, "vt ") && parseSpace(ptr)) // texture vertex
		{
			Vec2f v;
			if (parseFloats(ptr, v.getPtr(), 2) && parseSpace(ptr))
			{
				float dummy;
				while (parseFloat(ptr, dummy) && parseSpace(ptr));

				if (!*ptr)
				{
					s.texCoords.push_back(Vec2f(v.x, 1.0f - v.y));
					valid = true;
				}
			}
		}
		else if (parseLiteral(ptr, "vn ") && parseSpace(ptr)) // normal vertex
		{
			Vec3f v;
			if (parseFloats(ptr, v.getPtr(), 3) && parseSpace(ptr) && !*ptr)
			{
				s.normals.push_back(v);
				valid = true;
			}
		}
		else if (parseLiteral(ptr, "f ") && parseSpace(ptr)) // face
		{
			s.vertexTmp.clear();
			while (*ptr)
			{
				Vec3i ptn;
				if (!parseInt(ptr, ptn.x))
					break;
				for (int i = 1; i < 4 && parseLiteral(ptr, "/"); i++)
				{
					int tmp = 0;
					parseInt(ptr, tmp);
					if (i < 3)
						ptn[i] = tmp;
				}
				parseSpace(ptr);

				Vec3i size((int)s.positions.size(), (int)s.texCoords.size(), (int)s.normals.size());
				for (int i = 0; i < 3; i++)
				{
					if (ptn[i] < 0)
						ptn[i] += size[i];
					else
						ptn[i]--;

					if (ptn[i] < 0 || ptn[i] >= size[i])
						ptn[i] = -1;
				}

				int idx;
				bool found = s.vertexHash.search(ptn, idx);
				if (found)
					s.vertexTmp.push_back(idx);
				else
				{
					size_t vIdx = s.vertices.size();
					s.vertexTmp.push_back(s.vertexHash.add(ptn, (int)vIdx));
					s.vertices.push_back(ImportState::VertexPNT());
					ImportState::VertexPNT& v = s.vertices[vIdx];
					v.p = (ptn.x == -1) ? Vec3f(0.0f) : s.positions[ptn.x];
					v.t = (ptn.y == -1) ? Vec2f(0.0f) : s.texCoords[ptn.y];
					v.n = (ptn.z == -1) ? Vec3f(0.0f) : s.normals[ptn.z];
				}
			}
			if (!*ptr)
			{
				if (submesh == -1)
				{
					if (defaultSubmesh == -1)
						defaultSubmesh = s.addSubMesh();
					submesh = defaultSubmesh;
				}
				for (int i = 2; i < s.vertexTmp.size(); i++)
					s.indexTmp.push_back(Vec3i(s.vertexTmp[0], s.vertexTmp[i - 1], s.vertexTmp[i]));
				valid = true;
			}
		}
		else if (parseLiteral(ptr, "usemtl ") && parseSpace(ptr)) // material name
		{
			int mati = s.materialHash.searchi(std::string(ptr));
			if (submesh != -1)
			{
				push(s.subMeshes[submesh].indices, s.indexTmp);
				s.indexTmp.clear();
				submesh = -1;
			}
			if (mati != -1)
			{
				auto& mat = s.materialHash.vec[mati];
				if (mat.submesh == -1)
				{
					mat.submesh = s.addSubMesh();
					s.subMeshes[mat.submesh].material = mat;
				}
				submesh = mat.submesh;
				s.indexTmp.clear();
			}
			valid = true;
		}
		else if (parseLiteral(ptr, "mtllib ") && parseSpace(ptr) && *ptr) // material library
		{
			if (dirName.size())
			{
				std::string str = std::string(ptr);
				trim(str);
				std::string fileName = dirName + "/" + ptr;
				MemInputStream mtlIn(fileName.c_str());
				loadMtl(s, mtlIn, dirName);
				mtlIn.Close();
			}
			valid = true;
		}
		else if (
			parseLiteral(ptr, "vp ") ||         // parameter space vertex
			parseLiteral(ptr, "deg ") ||        // degree
			parseLiteral(ptr, "bmat ") ||       // basis matrix
			parseLiteral(ptr, "step ") ||       // step size
			parseLiteral(ptr, "cstype ") ||     // curve/surface type
			parseLiteral(ptr, "p ") ||          // point
			parseLiteral(ptr, "l ") ||          // line
			parseLiteral(ptr, "curv ") ||       // curve
			parseLiteral(ptr, "curv2 ") ||      // 2d curve
			parseLiteral(ptr, "surf ") ||       // surface
			parseLiteral(ptr, "parm ") ||       // curve/surface parameters
			parseLiteral(ptr, "trim ") ||       // curve/surface outer trimming loop
			parseLiteral(ptr, "hole ") ||       // curve/surface inner trimming loop
			parseLiteral(ptr, "scrv ") ||       // curve/surface special curve
			parseLiteral(ptr, "sp ") ||         // curve/surface special point
			parseLiteral(ptr, "end ") ||        // curve/surface end statement
			parseLiteral(ptr, "con ") ||        // surface connect
			parseLiteral(ptr, "g ") ||          // group name
			parseLiteral(ptr, "s ") ||          // smoothing group
			parseLiteral(ptr, "mg ") ||         // merging group
			parseLiteral(ptr, "o ") ||          // object name
			parseLiteral(ptr, "bevel ") ||      // bevel interpolation
			parseLiteral(ptr, "c_interp ") ||   // color interpolation
			parseLiteral(ptr, "d_interp ") ||   // dissolve interpolation
			parseLiteral(ptr, "lod ") ||        // level of detail
			parseLiteral(ptr, "shadow_obj ") || // shadow casting
			parseLiteral(ptr, "trace_obj ") ||  // ray tracing
			parseLiteral(ptr, "ctech ") ||      // curve approximation technique
			parseLiteral(ptr, "stech ") ||      // surface approximation technique
			parseLiteral(ptr, "g"))             // ???
		{
			valid = true;
		}

#if WAVEFRONT_DEBUG
		if (!valid)
			setError("Invalid line %d in Wavefront OBJ: '%s'!", lineNum, line);
#endif
	}

	// Flush remaining indices.

	if (submesh != -1)
		push(s.subMeshes[submesh].indices, s.indexTmp);
}

void compileobj(IInStream& in, FileOutputStream& a_Out)
{
	ImportState state;
	parse(state, in);

	if (state.subMeshes.size() == 0)
		throw std::runtime_error("Invalid obj file, did not find submeshes!");

	unsigned int numVertices = (unsigned int)state.vertices.size();

	std::vector<Vec3f> positions(numVertices);
	std::vector<Vec3f> normals(numVertices);
	std::vector<Vec2f> texCoords(numVertices);
	for (size_t i = 0; i < numVertices; i++)
	{
		auto& v = state.vertices[i];
		positions[i] = v.p;
		texCoords[i] = v.t;
		normals[i] = v.n;
	}
	std::vector<unsigned int> submeshes;
	std::vector<unsigned int> indices;
	std::vector<Spectrum> lights;
	std::vector<Material> matData;
	for (unsigned int submesh = 0; submesh < state.subMeshes.size(); submesh++)
	{
		const auto& M = state.subMeshes[submesh].material;
		Material mat(M.Name.c_str());
		float f = 0.0f;

		if (M.IlluminationModel == 2)
		{
			bool hasSpecular = !M.specular.isZero() || M.textures[TextureType_Specular].size() != 0;

			auto diff_tex = CreateTexture(M.textures[TextureType_Diffuse].c_str(), Spectrum(M.diffuse.x, M.diffuse.y, M.diffuse.z));
			if (hasSpecular)
			{
				auto spec_tex = CreateTexture(M.textures[TextureType_Specular].c_str(), Spectrum(M.specular.x, M.specular.y, M.specular.z));
				mat.bsdf.SetData(phong(diff_tex, spec_tex, CreateTexture(0, Spectrum(M.glossiness))));
			}
			else
			{
				mat.bsdf.SetData(diffuse(diff_tex));
			}
		}
		else if (M.IlluminationModel == 5)
		{
			mat.bsdf.SetData(conductor(Spectrum(0.0f), Spectrum(1.0f)));
		}
		else if (M.IlluminationModel == 7)
		{
			mat.bsdf.SetData(dielectric(M.IndexOfRefraction, CreateTexture(0, Spectrum(M.specular.x, M.specular.y, M.specular.z)), CreateTexture(0, Spectrum(M.Tf.x, M.Tf.y, M.Tf.z))));
		}
		else if (M.IlluminationModel == 9)
		{
			mat.bsdf.SetData(dielectric(M.IndexOfRefraction, CreateTexture(0, Spectrum(0.0f)), CreateTexture(0, Spectrum(M.Tf.x, M.Tf.y, M.Tf.z))));
		}

		if (M.textures[TextureType_Displacement].size())
		{
			mat.SetHeightMap(CreateTexture(M.textures[TextureType_Displacement].c_str(), Spectrum()));
		}
		if (M.textures[TextureType_Alpha].size() != 0)
		{
			mat.SetAlphaMap(CreateTexture(M.textures[TextureType_Alpha].c_str(), Spectrum()), AlphaBlendState::AlphaMap_Luminance);
			mat.AlphaMap.test_val_scalar = 1.0f;
		}


		if (length(M.emission))
			lights.push_back(Spectrum(M.emission.x, M.emission.y, M.emission.z));
		else lights.push_back(Spectrum(0.0f));
		matData.push_back(mat);

		auto nTriangles = state.subMeshes[submesh].indices.size();
		submeshes.push_back((unsigned int)nTriangles);
		indices.reserve(indices.size() + nTriangles * 3);
		for (auto& i : state.subMeshes[submesh].indices)
		{
			indices.push_back(i.z);
			indices.push_back(i.y);
			indices.push_back(i.x);
		}
	}

	std::vector<const Vec2f*> uv_sets = { texCoords.size() ? &texCoords[0] : 0};
	Mesh::CompileMesh(&positions[0], numVertices, state.normals.size() != 0 ? &normals[0] : 0, uv_sets.data(), uv_sets[0] ? 1 : 0 , &indices[0], (unsigned int)indices.size(), &matData[0], lights.size() ? &lights[0] : 0, &submeshes[0], 0, a_Out);
}

}