"""Generate offline all-items responses exceeding both former library caps."""
import json
import sys

fixture = {}
for status in ('watching', 'plantowatch', 'completed', 'hold', 'dropped'):
    response = {}
    for kind, key, count in [('movies', 'movie', 1003), ('shows', 'show', 1001), ('anime', 'show', 7)]:
        entries = []
        for i in range(count):
            number = (10000 if kind == 'anime' else 20000 if kind == 'movies' else 30000) + i
            ids = {'simkl': number}
            if i % 3 == 0:
                ids['imdb'] = f'tt{number}'
            elif i % 3 == 1:
                ids['tmdb'] = number
            entries.append({'status': status, key: {'title': f'{kind} {i}', 'year': 2020, 'ids': ids}})
        response[kind] = entries
    fixture[status] = response
with open(sys.argv[1], 'w') as output:
    json.dump(fixture, output)
