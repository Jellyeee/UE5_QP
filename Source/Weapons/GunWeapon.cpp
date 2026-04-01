#include "GunWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Projectiles/QPProjectileBullet.h"
#include "PJ_Quiet_Protocol/Character/Components/QPCombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"

AGunWeapon::AGunWeapon()
{
	WeaponType = EQPWeaponType::EWT_Rifle;
}

void AGunWeapon::BeginPlay()
{
	Super::BeginPlay();

	// 샷건과 권총은 기본적으로 단발(반자동) 무기로 취급하여 버튼을 꾹 눌러도 연사되지 않도록 설정
	if (WeaponType == EQPWeaponType::EWT_Shotgun || WeaponType == EQPWeaponType::EWT_Handgun)
	{
		bAutomatic = false;
	}

	// 무기 타입에 따른 기본 총알 설정 및 기본 연사 간격 조절
	if (WeaponType == EQPWeaponType::EWT_Rifle) 
	{
		MagCapacity = 30;
	}
	else if (WeaponType == EQPWeaponType::EWT_Handgun) 
	{
		MagCapacity = 10;
		if (FMath::IsNearlyEqual(FireRate, 0.15f)) FireRate = 0.3f; // 권총은 빠른 클릭 시 0.3초의 발사 간격을 가짐
	}
	else if (WeaponType == EQPWeaponType::EWT_Shotgun) 
	{
		MagCapacity = 4;
		if (FMath::IsNearlyEqual(FireRate, 0.15f)) FireRate = 0.8f; // 샷건은 빠른 클릭 시 0.8초의 발사 간격을 가짐
	}

	CurrentAmmo = MagCapacity;
}

void AGunWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGunWeapon, CurrentAmmo);
}

void AGunWeapon::StartFire_Implementation() //발사 시작 함수 재정의
{
	if (CurrentAmmo <= 0) return; // 잔탄이 없으면 발사하지 않음

	SpendRound(); // 발사할 때 장탄수 1 감소
	FireOnce();
	
	// 소음(총소리) 발생
	if (GetOwner())
	{
		// 플레이어가 뛰는 소리(0.5)를 기준으로 비율 설정
		float NoiseLoudness = 1.0f; 

		if (WeaponType == EQPWeaponType::EWT_Handgun) 
		{
			NoiseLoudness = 2.0f; // 권총: 뛰는 소리 2배
		}
		else if (WeaponType == EQPWeaponType::EWT_Rifle || WeaponType == EQPWeaponType::EWT_Shotgun)
		{
			NoiseLoudness = 4.0f; // 라이플 / 샷건: 뛰는 소리 4배
		}

		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), NoiseLoudness, Cast<APawn>(GetOwner()), 0.f, TEXT("WeaponNoise"));
	}
}

void AGunWeapon::StopAttack_Implementation() //공격 중지 함수 재정의
{
	// 기본 구현 (필요시 추가)
}

void AGunWeapon::FireOnce()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); //무기 소유자를 캐릭터로 캐스팅
	if (!OwnerCharacter) return; //소유자가 유효하지 않으면 반환

	PelletsFiredCount = 0; // 발사된 총알 수 초기화

	if (WeaponType == EQPWeaponType::EWT_Shotgun)
	{
		// [Fix] 샷건은 타이머 대신 한 프레임에 정해진 산탄 개수만큼 반복 발사하여 
		// 진짜 샷건처럼 여러 발이 동시에 부채꼴로 퍼져나가게 변경했습니다.
		for (int32 i = 0; i < ShotgunPelletCount; ++i)
		{
			FireSinglePellet();
		}
	}
	else
	{
		// 샷건이 아닌 무기(권총, 소총 등)는 단 한 발만 즉시 발사
		FireSinglePellet();
	}
}

void AGunWeapon::FireSinglePellet()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()); //무기 소유자를 캐릭터로 캐스팅
	if (!OwnerCharacter) return; //소유자가 유효하지 않으면 반환

	// 무기 타입에 따른 사거리(Range) 및 퍼짐 각도 설정
	float CurrentRange = Range;
	float CurrentSpreadAngle = 0.f;

	if (WeaponType == EQPWeaponType::EWT_Handgun)
	{
		CurrentRange = HandgunRange; // 권총은 사거리가 짧음
	}
	else if (WeaponType == EQPWeaponType::EWT_Shotgun)
	{
		CurrentSpreadAngle = ShotgunSpreadAngle; // 샷건 퍼짐 각도 적용
	}

	// 소유 캐릭터의 컴포넌트에서 QPCombatComponent를 찾아서 크로스헤어 타겟 정보(HitTarget)를 가져옴
	UQPCombatComponent* CombatComponent = OwnerCharacter->FindComponentByClass<UQPCombatComponent>();
	if (!CombatComponent) return;

	// 크로스헤어 타겟 (HitTarget)
	const FVector TraceEnd = CombatComponent->HitTarget;

	//Bullet Projectile Spawn
	FVector MuzzleLocation = GetActorLocation(); //기본 총구 위치를 액터 위치로 설정
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocketName))//무기 메쉬와 총구 소켓이 유효한 경우
	{
		MuzzleLocation = WeaponMesh->GetSocketLocation(MuzzleSocketName); //총구 소켓 위치 가져오기
	}

	// 총알 방향 계산 (총구에서 타겟까지의 방향 벡터)
	const FVector BaseBulletDir = (TraceEnd - MuzzleLocation).GetSafeNormal();
	FVector FinalBulletDir = BaseBulletDir;

	// 샷건일 경우 퍼짐(Spread) 적용
	if (CurrentSpreadAngle > 0.f)
	{
		// 원뿔 형태로 랜덤하게 방향 벡터를 틀어줌 (HalfAngle 형태이므로 FMath::DegreesToRadians 사용)
		float HalfAngleRad = FMath::DegreesToRadians(CurrentSpreadAngle);
		FinalBulletDir = FMath::VRandCone(BaseBulletDir, HalfAngleRad);
	}

	// 사거리 제한 적용 (목표점이 너무 멀면 방향만 유지하고 최대 사거리까지만 쏘도록)
	FVector FinalTarget = MuzzleLocation + (FinalBulletDir * CurrentRange);

	// DrawDebugLine(GetWorld(), MuzzleLocation, FinalTarget, FColor::Red, false, 2.0f, 0, 2.0f); //디버그 라인이 총구에서 나가도록 그리기

	FActorSpawnParameters SpawnParams; //스폰 파라미터 설정
	SpawnParams.Owner = OwnerCharacter; //소유자 설정
	SpawnParams.Instigator = OwnerCharacter; //인스티게이터 설정
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // 어떤 구조물에 겹쳐도 무조건 스폰되게 보장

	const FRotator SpawnRotation = FinalBulletDir.Rotation(); //발사 방향을 회전으로 변환
	AQPProjectileBullet* ProjectileBullet = GetWorld()->SpawnActor<AQPProjectileBullet>(ProjectileBulletClass, MuzzleLocation, SpawnRotation, SpawnParams); //투사체 불릿 스폰
	
	if (ProjectileBullet) 
	{
		ProjectileBullet->SetBulletVelocity(FinalBulletDir, BulletSpeed); //계산된 총알 방향으로 속도 설정
		ProjectileBullet->Damage = BaseDamage;
		ProjectileBullet->DamageTypeClass = DamageTypeClass;
	}

}

void AGunWeapon::AddAmmo(int32 AmountToAdd)
{
	CurrentAmmo = FMath::Clamp(CurrentAmmo + AmountToAdd, 0, MagCapacity);
}

void AGunWeapon::SpendRound()
{
	CurrentAmmo = FMath::Clamp(CurrentAmmo - 1, 0, MagCapacity);
}
