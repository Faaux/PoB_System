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

Image::~Image()
{
	// If we are loading right now, wait for it to finish
	// to not invalidate 'this' which is used by load_image.
	wait_for_load();
	if ( surface_ ) {
		SDL_FreeSurface( surface_ );
	}
}

void Image::wait_for_load() const
{
	if ( is_loaded_ ) return;
	if ( !is_loading_ ) return;

	// Wait synchronously
	image_load_task_.join();
}

cb::task<> Image::load_image()
{
	is_loading_ = true;
	auto state = state_t::instance;

	// Move to other thread and start loading
	co_await state->global_thread_pool.schedule();
	surface_ = IMG_Load( filename_.c_str() );
	if ( !surface_ ) {
		printf( "IMG_Load(%s): %s\n", filename_.c_str(), IMG_GetError() );
		is_loading_ = false;
		co_return;
	}

	width_ = surface_->w;
	height_ = surface_->h;
	is_loaded_ = true;
	is_loading_ = false;
}