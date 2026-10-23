"""Add the private diagnostic cluster to a generated copy, never the input XML."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

def prepare(source, destination):
    tree = ET.parse(source)
    root = tree.getroot()
    profile = next(node for node in root.findall('Profiles') if node.get('Name') == 'HomeAutomation')
    endpoint = next(node for node in root.iter('Endpoints') if node.get('Id') == '1')
    if any(node.get('Id') == '0xfc01' for node in profile.findall('Clusters')):
        raise ValueError('Diagnostic cluster already present')
    ET.SubElement(profile, 'Clusters', Name='Diagnostics', Id='0xfc01')
    ET.SubElement(endpoint, 'InputClusters', Cluster='Diagnostics',
                  RxAPDU='WSDCGQ01LM_DEV->apduZCL', Discoverable='true')
    tree.write(destination, encoding='UTF-8', xml_declaration=True)

if __name__ == '__main__':
    prepare(Path(sys.argv[1]), Path(sys.argv[2]))
