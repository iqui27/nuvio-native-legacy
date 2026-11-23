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
    # Anime feature films (anime_type "movie") belong to the Movies tab.
    response['anime'].append({'status': status, 'show': {
        'title': f'anime movie {status}', 'anime_type': 'movie', 'year': 2021,
        'ids': {'simkl': 50000, 'imdb': 'tt50000'}}})
    # Simkl may also return that same feature under movies; it must not
    # appear twice in the tab.
    if status in ('plantowatch', 'completed'):
        response['anime'].append({'status': status, 'show': {
            'title': 'anime movie also in movies', 'anime_type': 'movie', 'year': 2020,
            'ids': {'simkl': 20000, 'imdb': 'tt20000'}}})
    # dropped keeps the feature film as the only Movies-side entry.
    if status == 'dropped':
        response['movies'] = []
    fixture[status] = response
with open(sys.argv[1], 'w') as output:
    json.dump(fixture, output)
