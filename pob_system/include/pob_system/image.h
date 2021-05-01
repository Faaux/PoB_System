#pragma once
#include <tasks/task.h>

#include <atomic>
#include <memory>
#include <string_view>
#include <string>

struct SDL_Surface;

class Image
{
public:
	inline static const std::string_view ASYNC_FLAG = "ASYNC";
	inline static const std::string_view CLAMP_FLAG = "CLAMP";
	inline static const std::string_view MIPMAP_FLAG = "MIPMAP";

	Image( const char* filename, bool mipmaps, bool clamp, bool load_async );
	~Image();

	void wait_for_load() const;

	bool is_loading() const
	{
		return is_loaded_;
	}

	int width() const
	{
		if ( is_loaded_ ) return width_;
		return 0;
	}

	int height() const
	{
		if ( is_loaded_ ) return height_;
		return 0;
	}

private:
	cb::task<> load_image();

	std::string filename_;
	bool mipmaps_;	// Currently ignored
	bool clamp_;	// Currently ignored
	bool load_async_;

	cb::task<> image_load_task_;
	std::atomic< bool > is_loaded_ = false;
	std::atomic< bool > is_loading_ = false;
	SDL_Surface* surface_ = nullptr;
	int width_ = 0;
	int height_ = 0;
};

struct ImageHandle
{
	std::unique_ptr< Image > image;
};