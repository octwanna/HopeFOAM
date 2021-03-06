/*--------------------------------------------------------------------------------------
|     __  ______  ____  ______ |                                                       |
|    / / / / __ \/ __ \/ ____/ | HopeFOAM: High Order Parallel Extensible CFD Software |
|   / /_/ / / / / /_/ / __/    |                                                       |
|  / __  / /_/ / ____/ /___    |                                                       |
| /_/ /_/\____/_/   /_____/    | Copyright(c) 2017-2017 The Exercise Group, AMS, China.|
|                              |                                                       |
----------------------------------------------------------------------------------------

License
    This file is part of HopeFOAM, which is developed by Exercise Group, Innovation 
    Institute for Defence Science and Technology, the Academy of Military Science (AMS), China.

    HopeFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    HopeFOAM is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with HopeFOAM.  If not, see <http://www.gnu.org/licenses/>.

Author
    Xu Liyang (xucloud77@gmail.com)

\*---------------------------------------------------------------------------*/

#include "averageFlux.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
namespace Foam
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace dg
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
tmp<Field<Type>>
averageFlux<Type>::fluxCalculateWeak
(
    const dgGaussField<vector>& U,
    const dgGaussField<Type>& vf
)const
{
	const dgMesh& mesh = vf.mesh();

	const Field<vector>& gUf_O = U.ownerFaceField();
	const Field<vector>& gUf_N = U.neighborFaceField();
	const Field<Type>& gvf_O = vf.ownerFaceField();
	const Field<Type>& gvf_N = vf.neighborFaceField();

	tmp<Field<Type> > tAns
    (
        new Field<Type>(mesh.totalGaussFaceDof())
    );
    Field<Type>& Ans = tAns.ref();

    const dgTree<physicalCellElement>& cellPhysicalEleTree = mesh.cellElementsTree();
    for(dgTree<physicalCellElement>::leafIterator iter = cellPhysicalEleTree.leafBegin(); iter != cellPhysicalEleTree.end(); ++iter){
        List<shared_ptr<physicalFaceElement>>& faceEleInCell = iter()->value().faceElementList();

        forAll(faceEleInCell, faceI){
            if(faceEleInCell[faceI]->ownerEle_ != iter()) continue;
            label subSize = faceEleInCell[faceI]->nGaussPoints_;
            label subPos = faceEleInCell[faceI]->ownerFaceStart_;
            vectorList& faceNx = faceEleInCell[faceI]->faceNx_;

            SubField<Type> subAns(Ans, subSize, subPos);
            SubField<vector> subUf_O(gUf_O, subSize, subPos);
            SubField<vector> subUf_N(gUf_N, subSize, subPos);
            SubField<Type> subVf_O(gvf_O, subSize, subPos);
            SubField<Type> subVf_N(gvf_N, subSize, subPos);

            forAll(subAns, pointI){
                subAns[pointI] = ((faceNx[pointI] & subUf_O[pointI])*subVf_O[pointI]
                               +  (faceNx[pointI] & subUf_N[pointI])*subVf_N[pointI])*0.5;
            }
        }
    }

    return tAns;
}


template<class Type>
tmp<Field<Type>>
averageFlux<Type>::fluxCalculateWeak
(
    const GeometricDofField<vector, dgPatchField, dgGeoMesh>& dofU,
    const GeometricDofField<Type, dgPatchField, dgGeoMesh>& vf
)const
{
    const dgMesh& mesh = vf.mesh();

    const Field<vector>& gUf = dofU.internalField();
    const Field<Type>& gvf = vf.internalField();

    tmp<Field<Type> > tAns
    (
        new Field<Type>(mesh.totalGaussFaceDof())
    );
    Field<Type>& Ans = tAns.ref();

    Field<vector> Uf_O(mesh.maxDofPerFace());
    Field<vector> Uf_N(mesh.maxDofPerFace());
    Field<Type> Vf_O(mesh.maxDofPerFace());
    Field<Type> Vf_N(mesh.maxDofPerFace());
    Field<Type> tmpData(mesh.maxDofPerFace());
    Field<Type> tmpData2(mesh.maxGaussPerCell());
    const stdElement* faceEle1, *faceEle2;
    const labelList* faceDofMapping1, *faceDofMapping2;
    const denseMatrix<scalar>* faceVand;
    label dofPerFace, cellStart1, cellStart2;

    const dgTree<physicalCellElement>& cellPhysicalEleTree = mesh.cellElementsTree();
    for(dgTree<physicalCellElement>::leafIterator iter = cellPhysicalEleTree.leafBegin(); iter != cellPhysicalEleTree.end(); ++iter){
        List<shared_ptr<physicalFaceElement>>& faceEleInCell = iter()->value().faceElementList();

        forAll(faceEleInCell, faceI){
            if(faceEleInCell[faceI]->ownerEle_ != iter()) continue;

            faceEle1 = &(faceEleInCell[faceI]->ownerEle_->value().baseFunction());
            faceEle2 = (faceEleInCell[faceI]->isPatch_)?NULL:&(faceEleInCell[faceI]->neighborEle_->value().baseFunction());

            dofPerFace = faceEle1->nDofPerFace();
            cellStart1 = faceEleInCell[faceI]->ownerEle_->value().dofStart();
            faceDofMapping1 = &(faceEleInCell[faceI]->ownerDofMapping());
            for(int i=0; i<dofPerFace; i++){
                Uf_O[i] = gUf[cellStart1 + (*faceDofMapping1)[i]];
                Vf_O[i] = gvf[cellStart1 + (*faceDofMapping1)[i]];
            }

            if(faceEle2 == NULL){
                label patchI = faceEleInCell[faceI]->sequenceIndex().first();
                label shift = faceEleInCell[faceI]->sequenceIndex().second();
                for(int i=0; i<dofPerFace; i++){
                    Uf_N[i] = dofU.boundaryField()[patchI][shift + i];
                    Vf_N[i] = vf.boundaryField()[patchI][shift + i];
                }
            }
            else{
                if(dofPerFace != faceEle2->nDofPerFace()){
                    FatalErrorIn
                    (
                        "averageFlux<Type>::fluxCalculateWeak"
                    )   << "uncompatiable face size with owner = " << dofPerFace
                        << " while neighbor = " << faceEle2->nDofPerFace()
                        << abort(FatalError);
                }
                cellStart2 = faceEleInCell[faceI]->neighborEle_->value().dofStart();
                faceDofMapping2 = &(faceEleInCell[faceI]->neighborDofMapping());
                for(int i=0; i<dofPerFace; i++){
                    Uf_N[i] = gUf[cellStart2 + (*faceDofMapping2)[i]];
                    Vf_N[i] = gvf[cellStart2 + (*faceDofMapping2)[i]];
                }
            }

            label subSize = faceEleInCell[faceI]->nGaussPoints_;
            label subPos = faceEleInCell[faceI]->ownerFaceStart_;
            vectorList& faceNx = faceEleInCell[faceI]->faceNx_;

            //init data
            SubField<Type> subAns(Ans, subSize, subPos);
            for(int i=0; i<subSize; i++){
                subAns[i] = pTraits<Type>::zero;
            }
            faceVand = &(const_cast<stdElement*>(faceEle1)->gaussFaceInterpolateMatrix(faceEleInCell[faceI]->gaussOrder_, 0));
            for(int k=0; k<3; k++){
                for(int i=0; i<dofPerFace; i++){
                    tmpData[i] = (Uf_O[i][k]*Vf_O[i] + Uf_N[i][k]*Vf_N[i])*0.5;
                }
                SubList<Type> subTmpData(tmpData, dofPerFace);
                SubList<Type> subTmpData2(tmpData2, subSize);
                denseMatrix<Type>::MatVecMult(*faceVand, subTmpData, subTmpData2);
                for(int i=0; i<subSize; i++)
                    subAns[i] += faceNx[i][k] * subTmpData2[i];
            }
        }
    }

    return tAns;
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
template<class Type>
tmp<Field<Type>>
averageFlux<Type>::fluxCalculateWeak
(
    const dgGaussField<Type>& vf
)const
{
	const dgMesh& mesh = vf.mesh();
    //const typename Foam::GeometricField<Type, dgPatchField, dgGeoMesh>::Boundary& bData = vf.originField().boundaryField();

    const Field<Type>& gvf_O = vf.ownerFaceField();
    const Field<Type>& gvf_N = vf.neighborFaceField();

    tmp<Field<Type> > tAns
    (
        new Field<Type>(mesh.totalGaussFaceDof())
    );
    Field<Type>& Ans = tAns.ref();

    forAll(Ans, pointI){
        Ans[pointI] = (gvf_O[pointI] + gvf_N[pointI])*0.5;
    }

    return tAns;
}

} // End namespace dg

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
