#include "ayu/features/media_prefetch/media_prefetch.h"

#include "ayu/ayu_settings.h"
#include "data/data_auto_download.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_groups.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_web_page.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"

namespace AyuFeatures {
namespace {

constexpr auto kPrefetchDelay = crl::time(150);
constexpr auto kForgetAfter = 4096;

} // namespace

MediaPrefetch::MediaPrefetch(Collector collector)
: _collector(std::move(collector))
, _timer([=] { run(); }) {
}

void MediaPrefetch::schedule() {
	if (!_timer.isActive()) {
		_timer.callOnce(kPrefetchDelay);
	}
}

void MediaPrefetch::run() {
	const auto screens = AyuSettings::getInstance().mediaPrefetchScreens();
	if (screens <= 0 || !_collector) {
		return;
	}
	if (_done.size() > kForgetAfter) {
		_done.clear();
	}
	for (const auto &item : _collector(screens)) {
		const auto &groups = item->history()->owner().groups();
		if (const auto group = groups.find(item)) {
			for (const auto &grouped : group->items) {
				prefetch(grouped);
			}
		} else {
			prefetch(item);
		}
	}
}

void MediaPrefetch::prefetch(not_null<HistoryItem*> item) {
	if (!_done.emplace(item->fullId()).second) {
		return;
	}
	const auto media = item->media();
	if (!media) {
		return;
	}
	if (const auto photo = media->photo()) {
		prefetchPhoto(item, photo);
	} else if (const auto document = media->document()) {
		prefetchDocument(item, document);
	} else if (const auto page = media->webpage()) {
		if (const auto photo = page->photo) {
			prefetchPhoto(item, photo);
		} else if (const auto document = page->document) {
			prefetchDocument(item, document);
		}
	}
}

void MediaPrefetch::prefetchPhoto(
		not_null<HistoryItem*> item,
		not_null<PhotoData*> photo) {
	if (photo->isNull()
		|| photo->cancelled()
		|| photo->loading(Data::PhotoSize::Large)) {
		return;
	}
	if (const auto active = photo->activeMediaView()) {
		if (active->loaded()) {
			return;
		}
	}
	const auto peer = item->history()->peer;
	const auto &settings = item->history()->session().settings();
	if (!Data::AutoDownload::Should(settings.autoDownload(), peer, photo)) {
		return;
	}
	photo->load(item->fullId(), LoadFromCloudOrLocal, true);
}

void MediaPrefetch::prefetchDocument(
		not_null<HistoryItem*> item,
		not_null<DocumentData*> document) {
	const auto origin = Data::FileOrigin(item->fullId());
	const auto active = document->activeMediaView();
	if (document->hasThumbnail()
		&& !document->thumbnailLoading()
		&& !(active && active->thumbnail())) {
		document->loadThumbnail(origin);
	}
	if (document->sticker() || document->isAnimation()) {
		const auto media = active ? active : document->createMediaView();
		media->automaticLoad(origin, item);
	}
}

} // namespace AyuFeatures
