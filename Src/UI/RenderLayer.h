#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp> 
#include <boost/lexical_cast.hpp>

class RenderLayer {
protected:
	std::string m_layerName;
	bool m_open;
	boost::uuids::uuid uuid;
public:
	RenderLayer()
		:m_open(true)
	{
		boost::uuids::random_generator generator;
		uuid = generator();
	};
	RenderLayer(const std::string& name) 
		:m_layerName(name)
		,m_open(true)
	{
		boost::uuids::random_generator generator;
		uuid = generator();
	}
	virtual ~RenderLayer() = default;
	RenderLayer(const RenderLayer&) = delete;
	RenderLayer& operator=(const RenderLayer&) = delete;
	virtual void OnAttach() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnImGuiRender(bool fullScreen = false) = 0;
	virtual void OnDetach() = 0;
	void SetLayerName(const std::string& name) { m_layerName = name; }
	const std::string& GetLayerName() const { return m_layerName; }
	const auto GetUUID() const { return uuid; }
	bool IsOpen() const { return m_open; }
};