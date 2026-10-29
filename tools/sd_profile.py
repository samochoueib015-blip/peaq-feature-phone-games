"""Read an explicit local SD identity; never substitute a development card.

Keep input profiles under ignored local/. The four CID words must use the
same order as the target controller's response, not a reformatted card label.
"""
from pathlib import Path
import hashlib
import json


def _uint32(value, name):
    if isinstance(value, str):
        try:
            value = int(value, 0)
        except ValueError as exc:
            raise ValueError(name + ' must be an integer or a 0x-prefixed hex string') from exc
    if type(value) is not int or not 0 <= value <= 0xffffffff:
        raise ValueError(name + ' must be an unsigned 32-bit integer')
    return value


def load_sd_profile(path):
    """Return normalized {cid_words: list[int], capacity_sectors: int}."""
    data = json.loads(Path(path).read_text(encoding='utf-8-sig'))
    if not isinstance(data, dict) or set(data) != {'cid_words', 'capacity_sectors'}:
        raise ValueError('SD profile must contain exactly cid_words and capacity_sectors')
    words = data['cid_words']
    if not isinstance(words, list) or len(words) != 4:
        raise ValueError('cid_words must contain exactly four controller-order words')
    words = [_uint32(word, 'CID word') for word in words]
    if words == [0] * 4 or words == [0xffffffff] * 4:
        raise ValueError('CID must be an explicit card identity, not an empty response')
    sectors = _uint32(data['capacity_sectors'], 'capacity_sectors')
    if sectors == 0:
        raise ValueError('capacity_sectors must be positive')
    return {'cid_words': words, 'capacity_sectors': sectors}


def sd_profile_sha256(profile):
    """Stable fingerprint of normalized profile fields, independent of whitespace."""
    data = json.dumps(profile, sort_keys=True, separators=(',', ':')).encode('ascii')
    return hashlib.sha256(data).hexdigest()


def sd_profile_header(profile):
    words = ','.join('0x%08xu' % word for word in profile['cid_words'])
    return ('/* Generated from an explicit local SD profile. */\n'
            '#define PEAQ_SAVE_EXPECTED_CID {' + words + '}\n'
            '#define PEAQ_SAVE_EXPECTED_CAPACITY ' + str(profile['capacity_sectors']) + 'u\n')
