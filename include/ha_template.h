// Builds the Home Assistant Jinja template used by pull mode. HA renders it via
// POST /api/template into a payload-v2 JSON document (see TECHNICAL.md), so a
// whole refresh costs a single HTTP request.
#ifndef HA_TEMPLATE_H
#define HA_TEMPLATE_H

#include "settings.h"
#include <string>

// true when `id` looks like a valid HA entity id (domain.object_id)
bool validEntityId(const char *id);

std::string buildHaTemplate(const SourceMap &m);

// Template returning a JSON array of entities matching `query` (substring of
// id or name, case-insensitive), optionally restricted to `domain`.
std::string buildEntitySearchTemplate(const char *query, const char *domain, int limit = 40);

#endif
