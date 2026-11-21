#pragma once

#include "base/flat_set.h"
#include "base/timer.h"
#include "data/data_msg_id.h"

class HistoryItem;
class PhotoData;
class DocumentData;

namespace AyuFeatures {

class MediaPrefetch final {
public:
	using Collector = Fn<std::vector<not_null<HistoryItem*>>(int screens)>;

	explicit MediaPrefetch(Collector collector);

	void schedule();

private:
	void run();
	void prefetch(not_null<HistoryItem*> item);
	void prefetchPhoto(
		not_null<HistoryItem*> item,
		not_null<PhotoData*> photo);
	void prefetchDocument(
		not_null<HistoryItem*> item,
		not_null<DocumentData*> document);

	Collector _collector;
	base::Timer _timer;
	base::flat_set<FullMsgId> _done;

};

} // namespace AyuFeatures
