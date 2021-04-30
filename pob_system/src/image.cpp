#include <pob_system/image.h>
#include <pob_system/state.h>

#include <SDL_image.h>

#include <filesystem>

Image::Image( const char* filename, bool mipmaps, bool clamp, bool load_async )
	: filename_( filename ), mipmaps_( mipmaps ), clamp_( clamp ), load_async_( load_async )
{
	if ( !std::filesystem::is_regular_file( filename ) ) {
		printf( "File does not exist: %s\n", filename_.c_str() );
		return;
	}

	image_load_task_ = load_image();
}

cb::task<> Image::load_image()
{
	auto state = state_t::instance;

	// Move to other thread and start loading
	co_await state->tp.schedule();
	surface_ = IMG_Load( filename_.c_str() );
	if ( !surface_ ) {
		printf( "IMG_Load(%s): %s\n", filename_.c_str(), IMG_GetError() );
		co_return;
	}

	width_ = surface_->w;
	height_ = surface_->h;
	is_loaded_ = true;
}