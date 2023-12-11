import pytools as pt
import sys

nprocs = sys.argv[-1]

dims = sys.argv[1:-1]

for i in range(0,len(dims),3):
    x = int(dims[i].replace(',',''))
    y = int(dims[i+1].replace(',',''))
    z = int(dims[i+2].replace(',',''))
    decom = pt.vlsvfile.computeLegacyDomainDecomposition([x,y,z],int(nprocs))
    print("Legacy:", x,y,z, "over nprocs", nprocs, decom)
    # decom = pt.vlsvfile.computeDomainDecomposition([x,y,z],int(nprocs))
    # print(x,y,z, "over nprocs", nprocs, decom)
